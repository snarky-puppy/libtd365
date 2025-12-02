/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */
#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/ssl.hpp>
#include <map>
#include <string>
#include <td365/cookiejar.h>
#include <td365/http.h>

namespace td365 {
extern http_headers const no_headers;
extern http_headers const application_json_headers;

struct http_client {
    http_client(std::string host);

    virtual ~http_client() = default;

    http_client(const http_client &) = delete;

    http_client(http_client &&) = delete;

    http_client &operator=(const http_client &) = delete;

    http_client &operator=(http_client &&) = delete;

    http_response get(std::string_view target,
                      std::optional<http_headers> headers = std::nullopt);

    http_response post(std::string_view target,
                       std::optional<std::string> body = std::nullopt,
                       std::optional<http_headers> header = std::nullopt);

    http_headers &default_headers() { return default_headers_; };

    const cookiejar &jar() const { return jar_; }

  private:
    void ensure_connected();

    std::map<std::string, std::string> set_req_defaults(
        boost::beast::http::request<boost::beast::http::string_body> &req);

    http_response send(boost::beast::http::verb verb, std::string_view target,
                       std::optional<std::string> body,
                       std::optional<http_headers> headers);

    boost::asio::io_context io_context_;
    using stream_t = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;
    stream_t stream_;

    const std::string host_;
    cookiejar jar_;
    http_headers default_headers_;
};
} // namespace td365
