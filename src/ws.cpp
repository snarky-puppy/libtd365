/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#include <boost/asio/detached.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/lexical_cast.hpp>
#include <iostream>
#include <td365/constants.h>
#include <td365/utils.h>
#include <td365/ws.h>

namespace td365 {
namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
using tcp = net::ip::tcp;
namespace ssl = boost::asio::ssl;

bool is_debug_enabled() {
    static bool enabled = [] {
        auto value = std::getenv("DEBUG");
        return value ? boost::lexical_cast<bool>(value) : false;
    }();
    return enabled;
}

ws::ws() : using_ssl_(false) {}

void ws::connect(boost::urls::url url) {
    // Determine if we should use SSL based on the URL scheme
    using_ssl_ = (url.scheme() == "wss" || url.scheme() == "https");

    auto const endpoints = td_resolve(url.host(), (using_ssl_ ? "443" : "80"));

    if (using_ssl_) {
        // Create SSL WebSocket
        ssl_ws_ = std::make_unique<ssl_websocket_type>(io_context_, ssl_ctx());

        // Set a timeout on the operation
        beast::get_lowest_layer(*ssl_ws_).expires_after(
            std::chrono::seconds(30));

        // Connect synchronously
        beast::get_lowest_layer(*ssl_ws_).connect(endpoints);

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

        // Perform the SSL handshake synchronously
        ssl_ws_->next_layer().handshake(ssl::stream_base::client);

        // Turn off the timeout on the tcp_stream, because
        // the websocket stream has its own timeout system.
        beast::get_lowest_layer(*ssl_ws_).expires_never();

        // Set suggested timeout settings for the websocket
        ssl_ws_->set_option(websocket::stream_base::timeout::suggested(
            beast::role_type::client));

        // Perform the websocket handshake synchronously
        ssl_ws_->handshake(url.encoded_host_and_port(), "/");
    } else {
        // Create plain WebSocket
        plain_ws_ = std::make_unique<plain_websocket_type>(io_context_);

        // Set a timeout on the operation
        beast::get_lowest_layer(*plain_ws_)
            .expires_after(std::chrono::seconds(30));

        // Connect synchronously
        beast::get_lowest_layer(*plain_ws_).connect(endpoints);

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

        // Perform the websocket handshake synchronously
        plain_ws_->handshake(url.encoded_host_and_port(), "/");
    }
}

void ws::close() {
    if (using_ssl_) {
        get_lowest_layer(*ssl_ws_).expires_after(std::chrono::seconds(1));
        ssl_ws_->close(boost::beast::websocket::close_code::normal);
    } else {
        get_lowest_layer(*plain_ws_).expires_after(std::chrono::seconds(1));
        plain_ws_->close(boost::beast::websocket::close_code::normal);
    }
}

void ws::send(std::string_view message) {
    if (using_ssl_) {
        ssl_ws_->write(net::buffer(message));
    } else {
        plain_ws_->write(net::buffer(message));
    }
    if (is_debug_enabled()) {
        std::cout << ">> " << message << std::endl;
    }
}

std::pair<boost::system::error_code, std::string>
ws::read_message(std::optional<std::chrono::milliseconds> timeout) {
    beast::flat_buffer buffer;
    boost::system::error_code ec;

    if (using_ssl_) {
        if (timeout) {
            beast::get_lowest_layer(*ssl_ws_).expires_after(*timeout);
        }
        ssl_ws_->read(buffer, ec);
    } else {
        if (timeout) {
            beast::get_lowest_layer(*plain_ws_).expires_after(*timeout);
        }
        plain_ws_->read(buffer, ec);
    }

    if (ec) {
        return std::make_pair(ec, std::string{});
    }

    std::string buf(static_cast<const char *>(buffer.cdata().data()),
                    buffer.cdata().size());

    if (is_debug_enabled()) {
        std::cout << "<< " << buf << std::endl;
    }

    return std::make_pair(ec, std::move(buf));
}
} // namespace td365
