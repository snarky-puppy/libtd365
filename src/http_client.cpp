/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#include "http_client.h"
#include "constants.h"
#include "utils.h"
#include <boost/asio/connect.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <cassert>
#include <iostream>
#include <gzip.h>

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = net::ip::tcp;
using net::awaitable;
using net::use_awaitable;
namespace ssl = boost::asio::ssl;

constexpr auto port = "443";

http_client::http_client(const net::any_io_executor &executor,
                         std::string_view host)
  : executor_(executor), host_(host), stream_(executor, ssl_ctx()),
    jar_(host.data()) {
}

boost::asio::awaitable<void> http_client::ensure_connected() {
  if (!stream_.lowest_layer().is_open()) {
    boost::system::error_code ec;

    auto endpoints = co_await td_resolve(executor_, host_, port);
    co_await net::async_connect(stream_.lowest_layer(), endpoints, redirect_error(use_awaitable, ec));
    if (ec) {
      std::cerr << "connect to " << host_ << " failed: " << ec.message() << std::endl;
      throw ec;
    }

    stream_.lowest_layer().set_option(boost::asio::socket_base::keep_alive(true));

    int secs = 5;
    auto rv = setsockopt(stream_.lowest_layer().native_handle(), IPPROTO_TCP, TCP_KEEPALIVE, &secs, sizeof(secs));
    if (rv < 0) {
      std::cout << "setsockopt " << rv << " " << strerror(errno) << std::endl;
    }

    if (!SSL_set_tlsext_host_name(stream_.native_handle(), host_.c_str()))
      throw boost::system::system_error{
        static_cast<int>(ERR_get_error()), boost::asio::error::get_ssl_category()
      };

    co_await stream_.async_handshake(ssl::stream_base::client, redirect_error(use_awaitable, ec));
    if (ec) {
      std::cerr << "Handshake failed: " << ec.message() << std::endl;
      throw ec;
    }
  }
  co_return;
}

boost::asio::awaitable<response> http_client::send(request req,
                                                   headers headers) {
  co_await ensure_connected();

  if (!default_headers().empty()) {
    for (const auto &[name, value]: default_headers()) {
      req.insert(name, value);
    }
  }

  if (!headers.empty()) {
    for (const auto &[name, value]: headers) {
      req.insert(name, value);
    }
  }

  jar_.apply(req);

  //
  {
    boost::system::error_code ec;
    co_await http::async_write(stream_, req, redirect_error(use_awaitable, ec));
    if (ec) {
      std::cerr << "http::async_write failed: " << ec.message() << std::endl;
      throw ec;
    }
  }

  // Read the response.
  boost::beast::flat_buffer buffer;
  http::response<http::string_body> res;

  //
  {
    boost::system::error_code ec;
    co_await http::async_read(stream_, buffer, res, redirect_error(use_awaitable, ec));
    if (ec) {
      if (ec == http::error::end_of_stream) {
        stream_.lowest_layer().close();
        stream_ = stream_t(executor_, ssl_ctx());
      } else {
        std::cerr << "http::async_read failed: " << ec.message() << std::endl;
      }
      throw ec;
    }
  }
  jar_.update(res);

  if (res.count(http::field::content_encoding)) {
    std::string_view encoding = res[http::field::content_encoding];
    if (encoding.find("gzip") != std::string::npos) {
      std::string decompressed = decompress_gzip(res.body());
      res.body() = std::move(decompressed);
    }
  }

  co_return res;
}


void http_client::set_req_defaults(http::request<http::string_body> &req) {
  req.set(http::field::host, host_);
  req.set(http::field::user_agent, UserAgent);
  req.set(http::field::accept, "*/*");
  req.set(http::field::accept_language, "en-US,en;q=0.5");
  req.set("X-Requested-With", "XMLHttpRequest");
  req.set(http::field::content_type, "application/json; charset=utf-8");
  req.set(http::field::accept_encoding, "gzip");
  req.set(http::field::connection, "keep-alive");
}

boost::asio::awaitable<response> http_client::get(const std::string &path) {
  http::request<http::string_body> req{http::verb::get, path, 11};
  set_req_defaults(req);

  return send(std::move(req), headers());
}

boost::asio::awaitable<response>
http_client::post(const std::string &path, const std::string &content_type,
                  const std::string &body) {
  http::request<http::string_body> req{http::verb::post, path, 11};
  set_req_defaults(req);
  req.set(http::field::content_type, content_type);
  req.body() = body;
  req.prepare_payload();

  return send(std::move(req), headers());
}

boost::asio::awaitable<response>
http_client::post(const std::string &path,
                  const headers &hdrs) {
  http::request<http::string_body> req{http::verb::post, path, 11};
  set_req_defaults(req);
  req.prepare_payload();

  return send(std::move(req), hdrs);
}
