/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#include "http_client.h"

#include "constants.h"
#include "utils.h"
#include "verify.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/dynamic_body.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/beast/version.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/url/url.hpp>

namespace td365 {
    namespace net = boost::asio;
    namespace beast = boost::beast;
    namespace http = beast::http;
    using tcp = net::ip::tcp;
    using net::awaitable;
    namespace ssl = boost::asio::ssl;

    constexpr auto const kBodySizeLimit = 128U * 1024U * 1024U; // 128 M

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
        {to_string(http::field::content_type), "application/json; charset=utf-8"}
    };

    http_client::http_client(boost::asio::any_io_executor ex, std::string host)
        : stream_(ex, ssl_ctx()), host_(std::move(host)), jar_(host_ + ".cookies"),
          default_headers_(create_default_headers()) {
        default_headers_.emplace(to_string(http::field::host), host_);
    }

    awaitable<void> http_client::ensure_connected() {
        if (!stream_.lowest_layer().is_open()) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
            if (!SSL_set_tlsext_host_name(stream_.native_handle(),
                                          const_cast<char *>(host_.c_str()))) {
                throw boost::system::system_error{
                    {
                        static_cast<int>(::ERR_get_error()),
                        boost::asio::error::get_ssl_category()
                    }
                };
            }

            auto const ep = co_await td_resolve(host_, "443");
            co_await beast::get_lowest_layer(stream_).async_connect(*ep.begin());

            co_await stream_.async_handshake(ssl::stream_base::client);
        }

        co_return;
    }

    boost::asio::awaitable<http_response>
    http_client::send(boost::beast::http::verb verb, std::string_view target,
                      std::optional<std::string> body,
                      std::optional<http_headers> headers) {
        co_await ensure_connected();
        auto ex = co_await boost::asio::this_coro::executor;

        auto req = http::request<http::string_body>{verb, target, 11};

        if (!default_headers_.empty()) {
            for (const auto &[name, value]: default_headers_) {
                req.insert(name, value);
            }
        }

        if (headers.has_value()) {
            for (const auto &[name, value]: headers.value()) {
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

        try {
            co_await http::async_write(stream_, req, boost::asio::use_awaitable);

            auto p = http::response_parser<http::dynamic_body>{};
            p.eager(true);
            p.body_limit(kBodySizeLimit);

            auto buffer = beast::flat_buffer{};
            co_await http::async_read(stream_, buffer, p,
                                      boost::asio::use_awaitable);

            auto response = p.release();

            jar_.update(response);

            co_return response;
        } catch (const boost::beast::error_code &ec) {
            if (ec == http::error::end_of_stream) {
                stream_.lowest_layer().close();
                stream_ = stream_t(ex, ssl_ctx());
            } else {
                spdlog::error("http_client::send: {}", ec.message());
            }
            throw ec;
        }

        http_response resp{boost::beast::http::status::unknown, 11};
        co_return resp;
    }

    awaitable<http_response> http_client::get(std::string_view target,
                                              std::optional<http_headers> headers) {
        co_return co_await send(http::verb::get, target, std::nullopt, headers);
    }

    awaitable<http_response>
    http_client::post(std::string_view target, std::optional<std::string> body,
                      std::optional<http_headers> headers) {
        co_return co_await send(http::verb::post, target, body, headers);
    }
} // namespace td365
