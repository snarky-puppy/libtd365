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
#include <zlib.h>

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = net::ip::tcp;
using net::awaitable;
using net::use_awaitable;
namespace ssl = boost::asio::ssl;

constexpr auto port = "443";

// Helper function to decompress gzip-compressed data using zlib.
std::string decompress_gzip(const std::string &compressed_data) {
  // Initialize zlib stream
  z_stream zs = {0};
  zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressed_data.data()));
  zs.avail_in = compressed_data.size();
  
  // Use gzip mode (MAX_WBITS + 16)
  int result = inflateInit2(&zs, 16 + MAX_WBITS);
  assert(result == Z_OK && "Failed to initialize zlib for gzip decompression");
  
  // Initial output buffer size - we'll resize as needed
  const size_t chunk_size = 16384; // 16KB
  std::string decompressed;
  char outbuffer[chunk_size];
  
  // Decompress until no more data
  do {
    zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
    zs.avail_out = chunk_size;
    
    result = inflate(&zs, Z_NO_FLUSH);
    assert(result != Z_STREAM_ERROR && "Zlib stream error during decompression");
    
    switch (result) {
      case Z_NEED_DICT:
        result = Z_DATA_ERROR;
        [[fallthrough]];
      case Z_DATA_ERROR:
      case Z_MEM_ERROR:
        inflateEnd(&zs);
        assert(false && "Zlib decompression error");
        break;
    }
    
    // Append decompressed data
    size_t bytes_decompressed = chunk_size - zs.avail_out;
    if (bytes_decompressed > 0) {
      decompressed.append(outbuffer, bytes_decompressed);
    }
  } while (zs.avail_out == 0);
  
  // Clean up
  inflateEnd(&zs);
  return decompressed;
}

http_client::http_client(const net::any_io_executor &executor,
                         std::string_view host)
    : executor_(executor), host_(host), socket_(executor, ssl_ctx),
      jar_(host.data()) {}

boost::asio::awaitable<void> http_client::ensure_connected() {
  if (!socket_.lowest_layer().is_open()) {
    auto endpoints = co_await td_resolve(executor_, host_, port);
    co_await net::async_connect(socket_.next_layer(), endpoints, use_awaitable);
    co_await socket_.async_handshake(ssl::stream_base::client, use_awaitable);
  }
  co_return;
}

void http_client::set_req_defaults(http::request<http::string_body> &req) {
  req.set(http::field::host, host_);
  req.set(http::field::user_agent, UserAgent);
  req.set(http::field::connection, "keep-alive");
  req.set(http::field::accept_encoding, "gzip, deflate");
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

boost::asio::awaitable<response> http_client::send(request req,
                                                   headers headers) {
  co_await ensure_connected();

  if (!default_headers().empty()) {
    for (const auto &[name, value] : default_headers()) {
      req.insert(name, value);
    }
  }

  jar_.apply(req);

  co_await http::async_write(socket_, req, use_awaitable);

  // Read the response.
  boost::beast::flat_buffer buffer;
  http::response<http::string_body> res;
  co_await http::async_read(socket_, buffer, res, use_awaitable);

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
