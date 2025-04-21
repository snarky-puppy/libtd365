/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#ifndef WS_H
#define WS_H

#include <execution_ctx.h>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <string>
#include <string_view>

using websocket_type = boost::beast::websocket::stream<
    boost::asio::ssl::stream<boost::beast::tcp_stream> >;

class ws {
public:
    explicit ws(td_context_view ctx);

    boost::asio::awaitable<void> connect(
        const std::string &host,
        const std::string &port);

    boost::asio::awaitable<void> close();

    boost::asio::awaitable<void> send(std::string_view message);

    boost::asio::awaitable<std::pair<boost::system::error_code, std::string> >
    read_message();

private:
    td_context_view ctx_;
    websocket_type ws_;
};

#endif // WS_H
