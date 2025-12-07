/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/dynamic_body.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <td365/constants.h>
#include <td365/http_client.h>
#include <td365/utils.h>
#include <td365/verify.h>

template <class Body, class Fields>
void log_request_debug(const boost::beast::http::request<Body, Fields> &req) {
    spdlog::debug("----- HTTP Request -----");

    // Method
    spdlog::debug("{} {}", req.method_string(), req.target());

    // Headers
    for (const auto &h : req) {
        spdlog::debug("  {}: {}", h.name_string(), h.value());
    }

    // Body
    if constexpr (!std::is_same_v<Body, boost::beast::http::empty_body>) {
        if constexpr (std::is_same_v<Body, boost::beast::http::string_body>) {
            spdlog::debug("Body: {}", req.body());
        } else {
            spdlog::debug("Body: <{} bytes>", req.body().size());
        }
    } else {
        spdlog::debug("Body: <empty>");
    }

    spdlog::debug("---------------------------------");
}

template <class Response> void log_response_debug(const Response &res) {
    spdlog::debug("----- HTTP Response -----");

    spdlog::debug("HTTP/1.1 {} {}", static_cast<unsigned>(res.result_int()),
                  res.reason());
    for (const auto &h : res) {
        spdlog::debug("  {}: {}", h.name_string(), h.value());
    }

    spdlog::debug("Body: <{} bytes>", res.body().size());

    spdlog::debug("----------------------------------");
}

namespace td365 {
namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = net::ip::tcp;
namespace ssl = boost::asio::ssl;

constexpr auto const kBodySizeLimit = 128U * 1024U * 1024U; // 128 M

// FIXME: move to private header
extern bool is_debug_enabled();

auto create_default_headers() {
    http_headers hdrs;
    hdrs.emplace(to_string(http::field::user_agent), UserAgent);
    hdrs.emplace(to_string(http::field::accept), "*/*");
    hdrs.emplace(to_string(http::field::accept_language), "en-US,en;q=0.5");
    hdrs.emplace(to_string(http::field::accept_encoding), "gzip");
    hdrs.emplace(to_string(http::field::connection), "keep-alive");
    return hdrs;
}

const http_headers no_headers{};
const http_headers application_json_headers{
    {to_string(http::field::content_type), "application/json; charset=utf-8"}};

http_client::http_client(boost::urls::url url)
    : stream_(io_context_, ssl_ctx()), base_url_(std::move(url)),
      jar_(base_url_.host() + ".cookies"),
      default_headers_(create_default_headers()) {
    default_headers_.emplace(to_string(http::field::host), base_url_.host());
}

void http_client::ensure_connected() {
    if (!stream_.lowest_layer().is_open()) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
        if (!SSL_set_tlsext_host_name(
                stream_.native_handle(),
                const_cast<char *>(base_url_.host().c_str()))) {
            spdlog::error("Failed to set SNI Host \"{}\": {}", base_url_.host(),
                          ::ERR_error_string(::ERR_get_error(), nullptr));
            throw boost::system::system_error{
                {static_cast<int>(::ERR_get_error()),
                 boost::asio::error::get_ssl_category()}};
        }

        auto const endpoints = td_resolve(base_url_.host(), "443");
        boost::asio::connect(beast::get_lowest_layer(stream_), endpoints);

        stream_.handshake(ssl::stream_base::client);
    }
}

http_response http_client::send(boost::beast::http::verb verb,
                                std::string_view target,
                                std::optional<std::string> body,
                                std::optional<http_headers> headers) {
    ensure_connected();

    auto req = http::request<http::string_body>{verb, target, 11};

    if (!default_headers_.empty()) {
        for (const auto &[name, value] : default_headers_) {
            req.insert(name, value);
        }
    }

    if (headers.has_value()) {
        for (const auto &[name, value] : headers.value()) {
            req.insert(name, value);
        }
    }

    jar_.apply(req);

    if (body.has_value()) {
        req.body() = *body;
        req.prepare_payload();
    } else if (verb == http::verb::post) {
        req.set(http::field::content_length, "0");
    }

    if (is_debug_enabled()) {
        log_request_debug(req);
    }

    try {
        http::write(stream_, req);

        auto p = http::response_parser<http::dynamic_body>{};
        p.eager(true);
        p.body_limit(kBodySizeLimit);

        auto buffer = beast::flat_buffer{};
        http::read(stream_, buffer, p);

        auto response = p.release();

        jar_.update(response);

        if (is_debug_enabled()) {
            log_response_debug(response);
        }

        return response;
    } catch (const boost::beast::error_code &ec) {
        if (ec == http::error::end_of_stream) {
            stream_.lowest_layer().close();
            stream_ = stream_t(io_context_, ssl_ctx());
        } else {
            spdlog::error("http_client::send: {}", ec.message());
        }
        throw;
    }
}

http_response http_client::get(std::string_view target,
                               std::optional<http_headers> headers) {
    return send(http::verb::get, target, std::nullopt, headers);
}

http_response http_client::post(std::string_view target,
                                std::optional<std::string> body,
                                std::optional<http_headers> headers) {
    return send(http::verb::post, target, body, headers);
}
} // namespace td365
