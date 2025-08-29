/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#include <td365/ws.h>

#include <td365/constants.h>
#include <td365/utils.h>

#include <boost/asio/detached.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/lexical_cast.hpp>
#include <iostream>

namespace td365 {
    namespace net = boost::asio;
    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace websocket = beast::websocket;
    using tcp = net::ip::tcp;
    using net::awaitable;
    using net::use_awaitable;
    namespace ssl = boost::asio::ssl;

    bool is_debug_enabled() {
        static bool enabled = [] {
            auto value = std::getenv("DEBUG");
            return value ? boost::lexical_cast<bool>(value) : false;
        }();
        return enabled;
    }

    ws::ws() : using_ssl_(false) {
    }

    boost::asio::awaitable<void> ws::connect(boost::urls::url url) {
        auto executor = co_await net::this_coro::executor;

        // Determine if we should use SSL based on the URL scheme
        using_ssl_ = (url.scheme() == "wss" || url.scheme() == "https");

        auto const ep = co_await td_resolve(url.host(), (using_ssl_ ? "443" : "80"));

        if (using_ssl_) {
            // Create SSL WebSocket
            ssl_ws_ = std::make_unique<ssl_websocket_type>(executor, ssl_ctx());

            // Set a timeout on the operation
            beast::get_lowest_layer(*ssl_ws_).expires_after(
                std::chrono::seconds(30));

            co_await beast::get_lowest_layer(*ssl_ws_).async_connect(ep, use_awaitable);

            // Set SNI Hostname (many hosts need this to handshake successfully)
            if (!SSL_set_tlsext_host_name(ssl_ws_->next_layer().native_handle(),
                                          url.host().c_str())) {
                throw beast::system_error(static_cast<int>(::ERR_get_error()),
                                          net::error::get_ssl_category());
            }

            // Set a timeout on the operation
            beast::get_lowest_layer(*ssl_ws_).expires_after(
                std::chrono::seconds(30));

            // Set a decorator to change the User-Agent of the handshake
            ssl_ws_->set_option(
                websocket::stream_base::decorator([](websocket::request_type &req) {
                    req.set(http::field::user_agent, UserAgent);
                }));

            // Perform the SSL handshake
            co_await ssl_ws_->next_layer().async_handshake(
                ssl::stream_base::client, use_awaitable);

            // Turn off the timeout on the tcp_stream, because
            // the websocket stream has its own timeout system.
            beast::get_lowest_layer(*ssl_ws_).expires_never();

            // Set suggested timeout settings for the websocket
            ssl_ws_->set_option(websocket::stream_base::timeout::suggested(
                beast::role_type::client));

            // Perform the websocket handshake
            co_await ssl_ws_->async_handshake(url.encoded_host_and_port(), "/",
                                              use_awaitable);
        } else {
            // Create plain WebSocket
            plain_ws_ = std::make_unique<plain_websocket_type>(executor);

            // Set a timeout on the operation
            beast::get_lowest_layer(*plain_ws_)
                    .expires_after(std::chrono::seconds(30));

            co_await beast::get_lowest_layer(*plain_ws_).async_connect(ep, use_awaitable);

            // Set a decorator to change the User-Agent of the handshake
            plain_ws_->set_option(
                websocket::stream_base::decorator([](websocket::request_type &req) {
                    req.set(http::field::user_agent, UserAgent);
                }));

            // Turn off the timeout on the tcp_stream, because
            // the websocket stream has its own timeout system.
            beast::get_lowest_layer(*plain_ws_).expires_never();

            // Set suggested timeout settings for the websocket
            plain_ws_->set_option(websocket::stream_base::timeout::suggested(
                beast::role_type::client));

            // Perform the websocket handshake
            co_await plain_ws_->async_handshake(url.encoded_host_and_port(), "/",
                                                use_awaitable);
        }

        co_return;
    }

    boost::asio::awaitable<void> ws::close() {
        if (using_ssl_) {
            get_lowest_layer(*ssl_ws_).expires_after(std::chrono::seconds(1));
            co_await ssl_ws_->async_close(
                boost::beast::websocket::close_code::normal, use_awaitable);
        } else {
            get_lowest_layer(*plain_ws_).expires_after(std::chrono::seconds(1));
            co_await plain_ws_->async_close(
                boost::beast::websocket::close_code::normal, use_awaitable);
        }
        co_return;
    }

    boost::asio::awaitable<void> ws::send(std::string_view message) {
        if (using_ssl_) {
            co_await ssl_ws_->async_write(net::buffer(message), use_awaitable);
        } else {
            co_await plain_ws_->async_write(net::buffer(message), use_awaitable);
        }
        if (is_debug_enabled()) {
            std::cout << ">> " << message << std::endl;
        }

        co_return;
    }

    boost::asio::awaitable<std::pair<boost::system::error_code, std::string> >
    ws::read_message() {
        beast::flat_buffer buffer;
        boost::system::error_code ec;

        if (using_ssl_) {
            co_await ssl_ws_->async_read(
                buffer, boost::asio::redirect_error(use_awaitable, ec));
        } else {
            co_await plain_ws_->async_read(
                buffer, boost::asio::redirect_error(use_awaitable, ec));
        }

        if (ec) {
            co_return std::make_pair(ec, std::string{});
        }

        std::string buf(static_cast<const char *>(buffer.cdata().data()),
                        buffer.cdata().size());

        if (is_debug_enabled()) {
            std::cout << "<< " << buf << std::endl;
        }

        co_return std::make_pair(ec, std::move(buf));
    }
} // namespace td365
