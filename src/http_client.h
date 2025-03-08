/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include "cookiejar.h"
#include "http.h"
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/http.hpp>
#include <string>

class http_client {
public:
  http_client(const boost::asio::any_io_executor &executor,
              std::string_view host);

  boost::asio::awaitable<response> get(const std::string &path);

  boost::asio::awaitable<response> post(const std::string &path,
                                        const std::string &content_type,
                                        const std::string &body);

  void set_default_headers(headers headers) {
    default_headers_ = std::move(headers);
  };
  headers &default_headers() { return default_headers_; };

  const cookiejar &jar() const { return jar_; }

private:
  boost::asio::awaitable<void> ensure_connected();

  void set_req_defaults(
      boost::beast::http::request<boost::beast::http::string_body> &req);

  boost::asio::awaitable<response> send(request req, headers headers);

  boost::asio::any_io_executor executor_;
  std::string host_;
  boost::asio::ssl::stream<boost::asio::ip::tcp::socket> socket_;

  headers default_headers_;
  cookiejar jar_;
};

#endif // HTTP_CLIENT_H
