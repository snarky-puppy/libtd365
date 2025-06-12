/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#include "ws.h"

#include "constants.h"
#include "utils.h"

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

ws::ws() {}

boost::asio::awaitable<void> ws::connect(boost::urls::url url) {
    auto executor = co_await net::this_coro::executor;
    auto resolver = boost::asio::ip::tcp::resolver{executor};

    auto u = url;
    if (auto *env = std::getenv("PROXY")) {
        u = boost::urls::url{env};
    }

    ws_ = std::make_unique<websocket_type>(executor, ssl_ctx());

    auto const ep = co_await resolver.async_resolve(
        u.host(), u.has_port() ? u.port() : "443");

    // Set a timeout on the operation
    beast::get_lowest_layer(*ws_).expires_after(std::chrono::seconds(30));

    // Make the connection on the IP address we get from a lookup
    co_await beast::get_lowest_layer(*ws_).async_connect(ep);

    // Set SNI Hostname (many hosts need this to handshake successfully)
    if (!SSL_set_tlsext_host_name(ws_->next_layer().native_handle(),
                                  u.host().c_str())) {
        throw beast::system_error(static_cast<int>(::ERR_get_error()),
                                  net::error::get_ssl_category());
    }

    // Set a timeout on the operation
    beast::get_lowest_layer(*ws_).expires_after(std::chrono::seconds(30));

    // Set a decorator to change the User-Agent of the handshake
    ws_->set_option(
        websocket::stream_base::decorator([](websocket::request_type &req) {
            req.set(http::field::user_agent, UserAgent);
        }));

    // Perform the SSL handshake
    co_await ws_->next_layer().async_handshake(ssl::stream_base::client);

    // Turn off the timeout on the tcp_stream, because
    // the websocket stream has its own timeout system.
    beast::get_lowest_layer(*ws_).expires_never();

    // Set suggested timeout settings for the websocket
    ws_->set_option(
        websocket::stream_base::timeout::suggested(beast::role_type::client));

    // Perform the websocket handshake
    co_await ws_->async_handshake(u.host(), "/", use_awaitable);

    co_return;
}

boost::asio::awaitable<void> ws::close() {
    get_lowest_layer(*ws_).expires_after(std::chrono::seconds(1));
    co_await ws_->async_close(boost::beast::websocket::close_code::normal);
    co_return;
}

boost::asio::awaitable<void> ws::send(std::string_view message) {
    co_await ws_->async_write(net::buffer(message));
    if (is_debug_enabled()) {
        std::cout << ">> " << message << std::endl;
    }

    co_return;
}

boost::asio::awaitable<std::pair<boost::system::error_code, std::string>>
ws::read_message() {
    beast::flat_buffer buffer;
    boost::system::error_code ec;

    co_await ws_->async_read(buffer,
                             boost::asio::redirect_error(use_awaitable, ec));

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
