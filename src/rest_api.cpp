/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#include <boost/beast.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <td365/error.h>
#include <td365/http_client.h>
#include <td365/parsing.h>
#include <td365/rest_api.h>
#include <td365/types.h>
#include <td365/utils.h>
#include <td365/verify.h>

namespace beast = boost::beast;   // from <boost/beast.hpp>
namespace http = beast::http;     // from <boost/beast/http.hpp>
namespace net = boost::asio;      // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl; // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>
using json = nlohmann::json;

static constexpr auto MAX_DEPTH = 4;

namespace td365 {
namespace {
std::string extract_ots(std::string_view s) {
    auto r = boost::urls::parse_origin_form(s);
    auto u = r.value();
    auto iter = u.encoded_params().find("ots");
    verify(iter != u.encoded_params().end(),
           "extract_ots: missing parameter in '{}'", s);
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

std::string extract_account_id(std::string_view body) {
    constexpr std::string_view key = R"(id="hfAccountID" value=")";
    auto pos = body.find(key);
    verify(pos != std::string_view::npos,
           "could not find hfAccountID in document");
    pos += key.size();
    auto end = body.find('"', pos);
    verify(end != std::string_view::npos, "hfAccountID element badly formed");
    return std::string{body.substr(pos, end - pos)};
}

template <typename T> T extract_d(const json &j) {
    return j.at("d").template get<T>();
}

template <typename T>
auto make_post(http_client *client, std::string_view target,
               std::optional<std::string> body,
               std::optional<http_headers> headers = std::nullopt) -> T {
    auto resp = client->post(target, std::move(body), std::move(headers));
    verify(resp.result() == boost::beast::http::status::ok,
           "unexpected response: from {}: {}", target,
           static_cast<unsigned>(resp.result()));
    auto j = json::parse(get_http_body(resp));
    return extract_d<T>(j);
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

auto rest_api::open_client(std::string_view target, int depth)
    -> std::pair<std::string, std::string> {
    std::string t(target);

    while (depth <= MAX_DEPTH) {
        spdlog::info("Following link: {}", t);
        auto response = client_->get(t);
        if (response.result() == http::status::ok) {
            // extract the ots value here while we have the path
            // GET /Advanced.aspx?ots=WJFUMNFE
            // ots is the name of the cookie with the session token
            auto ots = extract_ots(t);
            auto body = get_http_body(response);
            auto login_id = extract_login_id(body);
            account_id_ = extract_account_id(body);
            get_market_details_url_ = std::format(
                "/UTSAPI.asmx/GetMarketDetails?AccountID={}", account_id_);
            return std::make_pair(ots, login_id);
        }
        verify(response.result() == http::status::found,
               "unexpected response from {}: result={}", t,
               static_cast<unsigned>(response.result()));

        t = response.at(http::field::location);
        depth++;
    }
    throw fail("max depth reached: {}", target);
}

auto rest_api::connect(boost::urls::url url) -> rest_api::auth_info {
    client_ = std::make_unique<http_client>(url.host());
    spdlog::info("Opening {}", url.buffer());
    auto [ots, login_id] = open_client(url.encoded_target());
    auto token = client_->jar().get(ots);

    const auto referer =
        std::format("{}://{}/Advanced.aspx?ots={}", std::string{url.scheme()},
                    url.host(), ots);

    std::string origin =
        std::format("{}://{}", std::string(url.scheme()), url.host());

    client_->default_headers().emplace("Origin", origin);
    client_->default_headers().emplace("Referer", referer);
    client_->default_headers().emplace("Content-Type",
                                       "application/json; charset=utf-8");
    client_->default_headers().emplace("X-Requested-With", "XMLHttpRequest");
    return auth_info{token.value, login_id};
}

auto rest_api::get_market_super_group() -> std::vector<market_group> {
    return make_post<std::vector<market_group>>(
        client_.get(), "/UTSAPI.asmx/GetMarketSuperGroup", std::nullopt);
}

auto rest_api::get_market_group(int super_group_id)
    -> std::vector<market_group> {
    json body = {{"superGroupId", super_group_id}};
    return make_post<std::vector<market_group>>(
        client_.get(), "/UTSAPI.asmx/GetMarketGroup", body.dump());
}

auto rest_api::get_market_quote(int group_id) -> std::vector<market> {
    json body = {
        {"groupID", group_id}, {"keyword", ""},   {"popular", false},
        {"portfolio", false},  {"search", false},
    };
    return make_post<std::vector<market>>(
        client_.get(), "/UTSAPI.asmx/GetMarketQuote", body.dump());
}

auto rest_api::get_market_details(int market_id) -> market_details_response {
    json body = {{"marketID", market_id}};
    return make_post<market_details_response>(
        client_.get(), get_market_details_url_, body.dump());
}

// auto rest_api::get_chart_url(int market_id) -> awaitable<boost::urls::url> {
// json body = {{"getAdvancedChart", false}, {"marketID", market_id}};
// co_return co_await make_post<boost::urls::url>(
// client_.get(), "/UTSAPI.asmx/GetChartURL", body.dump());
// }

auto rest_api::backfill(int market_id, int /*quote_id*/, size_t sz,
                        chart_duration /*dur*/) -> std::vector<candle> {
    // auto chart_url = get_chart_url(market_id);
    // spdlog::info("chart url: {}", chart_url.buffer());

    // FIXME
    auto hc = http_client("charts.finsatechnology.com");
    auto target = std::format("/data/minute/{}/mid?l={}", market_id, sz);
    auto response = hc.get(target);
    auto j = json::parse(get_http_body(response));
    auto data = j.at("data").get<std::vector<std::string>>();
    auto rv = std::vector<candle>(sz);
    for (size_t i = 0; i < sz; ++i) {
        rv[i] = parse_candle(data[i]);
    }
    return rv;
}

auto rest_api::trade(const trade_request &request) -> trade_response {
    json body = {{"marketID", request.market_id},
                 {"quoteID", request.quote_id},
                 {"price", request.price},
                 {"stake", std::to_string(request.stake)},
                 {"tradeType", 1},
                 {"tradeMode", request.dir == trade_request::direction::sell},
                 {"hasClosingOrder", true},
                 {"isGuaranteed", false},
                 {"orderModeID", 3},
                 {"orderTypeID", 2},
                 {"orderPriceModeID", 2},
                 {"limitOrderPrice", std::to_string(request.limit)},
                 {"stopOrderPrice", std::to_string(request.stop)},
                 {"trailingPoint", 0},
                 {"closePositionID", 0},
                 {"isKaazingFeed", true},
                 {"userAgent", "Firefox (139.0)"},
                 {"key", request.key}};

    auto result = make_post<trade_response>(
        client_.get(), "/UTSAPI.asmx/RequestTrade", body.dump());
    return result;
}

auto rest_api::sim_trade(const trade_request &request) -> void {
    json body = {{"marketID", request.market_id},
                 {"quoteID", request.quote_id},
                 {"price", request.price},
                 {"stake", std::to_string(request.stake)},
                 {"tradeType", 1},
                 {"tradeMode", request.dir == trade_request::direction::sell},
                 {"hasClosingOrder", true},
                 {"isGuaranteed", false},
                 {"orderModeID", 3},
                 {"orderTypeID", 2},
                 {"orderPriceModeID", 2},
                 {"limitOrderPrice", std::to_string(request.limit)},
                 {"stopOrderPrice", std::to_string(request.stop)},
                 {"trailingPoint", 0},
                 {"closePositionID", 0},
                 {"isKaazingFeed", true},
                 {"userAgent", "Firefox (139.0)"},
                 {"key", request.key}};
    make_post<trade_response>(client_.get(),
                              "/UTSAPI.asmx/RequestTradeSimulate", body.dump());
}
} // namespace td365
