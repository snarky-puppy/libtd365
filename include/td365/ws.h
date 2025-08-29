/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/url/url.hpp>
#include <string>
#include <string_view>

namespace td365 {
using ssl_websocket_type = boost::beast::websocket::stream<
    boost::beast::ssl_stream<boost::beast::tcp_stream>>;
using plain_websocket_type =
    boost::beast::websocket::stream<boost::beast::tcp_stream>;

class ws {
  public:
    explicit ws();

    boost::asio::awaitable<void> connect(boost::urls::url);

    boost::asio::awaitable<void> close();

    boost::asio::awaitable<void> send(std::string_view message);

    boost::asio::awaitable<std::pair<boost::system::error_code, std::string>>
    read_message();

  private:
    std::unique_ptr<ssl_websocket_type> ssl_ws_;
    std::unique_ptr<plain_websocket_type> plain_ws_;
    bool using_ssl_;
};
} // namespace td365
