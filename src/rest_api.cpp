/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#include "rest_api.h"

#include "error.h"
#include "http_client.h"
#include "types.h"
#include "utils.h"
#include "verify.h"

#include <boost/beast.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace beast = boost::beast;   // from <boost/beast.hpp>
namespace http = beast::http;     // from <boost/beast/http.hpp>
namespace net = boost::asio;      // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl; // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>
using json = nlohmann::json;
using net::awaitable;

static constexpr auto MAX_DEPTH = 4;

namespace td365 {

namespace {
std::string extract_ots(const boost::urls::url &u) {
    constexpr std::string_view key = "ots";
    auto iter = u.encoded_params().find(key);
    verify(iter != u.encoded_params().end(),
           "extract_ots: missing parameter in '{}'", u.buffer());
    return std::string(iter->value);
}

std::string extract_login_id(std::string_view body) {
    constexpr std::string_view key = R"(id="hfLoginID" value=")";
    auto pos = body.find(key);
    verify(pos != std::string_view::npos,
           "could not find hfLoginID in document");
    pos += key.size();
    auto end = body.find('"', pos);
    verify(end != std::string_view::npos, "hfLoginID element badly formed");
    return std::string{body.substr(pos, end - pos)};
}

template <typename T> T extract_d(const json &j) {
    return j.at("d").template get<T>();
}

template <typename T>
auto make_post(http_client *client, const boost::urls::url &url,
               nlohmann::json &&body) -> net::awaitable<T> {
    auto resp = co_await client->post(url, body.dump());
    verify(resp.result() == boost::beast::http::status::ok,
           "unexpected response: from {}: {}", url.buffer(),
           static_cast<unsigned>(resp.result()));
    auto j = json::parse(get_http_body(resp));
    co_return extract_d<T>(j);
}

// void check_session_status(
// const boost::beast::http::message<
// false, boost::beast::http::basic_string_body<char>> &resp) {
// auto j = json::parse(resp.body());
// auto status = j["d"]["Status"].get<int>();
// verify(status == 0, "non-zero session status: {}", status);
// }

} // namespace

rest_api::rest_api() = default;

rest_api::~rest_api() = default;

auto rest_api::open_client(boost::urls::url u, int depth)
    -> net::awaitable<std::pair<std::string, std::string>> {
    if (depth > MAX_DEPTH) {
        spdlog::error("max depth reached: {}", depth);
        throw_api_error(api_error::max_depth, "path={} depth={}", u.buffer(),
                        depth);
    }
    auto response = co_await client_->get(u);
    if (response.result() == http::status::ok) {
        // extract the ots value here while we have the path
        // GET /Advanced.aspx?ots=WJFUMNFE
        // ots is the name of the cookie with the session token
        auto ots = extract_ots(u);
        auto login_id = extract_login_id(get_http_body(response));
        co_return std::make_pair(ots, login_id);
    }
    verify(response.result() == http::status::found,
           "unexpected response from {}: result={}", u.buffer(),
           static_cast<unsigned>(response.result()));

    const auto location = boost::urls::url{response.at(http::field::location)};
    co_return co_await open_client(location, depth + 1);
}

auto rest_api::connect(boost::urls::url url) -> awaitable<rest_api::auth_info> {
    auto ex = co_await net::this_coro::executor;
    client_ = std::make_unique<http_client>(ex, url.host());
    auto [ots, login_id] = co_await open_client(url);
    auto token = client_->jar().get(ots);

    const auto referer = std::format("{}://{}/Advanced.aspx?ots={}",
                                     url.scheme(), url.host(), ots);

    client_->default_headers().emplace("Origin", url.buffer());
    client_->default_headers().emplace("Referer", referer);
    co_return auth_info{token.value, login_id};
}

auto rest_api::get_market_super_group()
    -> awaitable<std::vector<market_group>> {
    json body = {};
    return make_post<std::vector<market_group>>(
        client_.get(), boost::urls::url{"/UTSAPI.asmx/GetMarketSuperGroup"},
        std::move(body));
}

auto rest_api::get_market_group(int id)
    -> awaitable<std::vector<market_group>> {
    json body = {{"superGroupId", id}};
    return make_post<std::vector<market_group>>(
        client_.get(), boost::urls::url{"/UTSAPI.asmx/GetMarketGroup"},
        std::move(body));
}

auto rest_api::get_market_quote(int id) -> awaitable<std::vector<market>> {
    json body = {
        {"groupID", id},      {"keyword", ""},   {"popular", false},
        {"portfolio", false}, {"search", false},
    };
    return make_post<std::vector<market>>(
        client_.get(), boost::urls::url{"/UTSAPI.asmx/GetMarketQuote"},
        std::move(body));
}
} // namespace td365
