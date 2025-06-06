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
#include <boost/url/url_view.hpp>
#include <string>
#include <string_view>

namespace td365 {
using websocket_type = boost::beast::websocket::stream<
    boost::asio::ssl::stream<boost::beast::tcp_stream>>;

class ws {
  public:
    explicit ws();

    boost::asio::awaitable<void> connect(boost::urls::url_view);

    boost::asio::awaitable<void> close();

    boost::asio::awaitable<void> send(std::string_view message);

    boost::asio::awaitable<std::pair<boost::system::error_code, std::string>>
    read_message();

  private:
    std::unique_ptr<websocket_type> ws_;
};
} // namespace td365
