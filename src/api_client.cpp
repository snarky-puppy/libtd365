/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#include "api_client.h"
#include "json.h"
#include "utils.h"
#include <cassert>
#include <nlohmann/json.hpp>
#include <regex>

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http; // from <boost/beast/http.hpp>
namespace net = boost::asio; // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl; // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>
namespace net = boost::asio;
using json = nlohmann::json;

api_client::api_client(const net::any_io_executor &executor,
                       std::string_view host)
    : client_(executor, host) {
}

boost::asio::awaitable<std::pair<std::string, std::string> > api_client::connect(std::string path) {
    auto response = co_await client_.get(path);
    if (response.result() == boost::beast::http::status::ok) {
        // extract the ots value here while we have the path
        // GET /Advanced.aspx?ots=WJFUMNFE
        // ots is the name of the cookie with the session token
        static std::regex ots_re(R"(^.*?ots=(.*)$)");
        std::smatch m;
        assert(std::regex_match(path, m, ots_re));
        auto ots = m[1].str();

        auto body = response.body();
        static std::regex login_id_re(R"(id=\"hfLoginID\" value=\"([^\"]+)\")");
        assert(std::regex_search(body, m, login_id_re));
        auto login_id = m[1].str();
        co_return std::make_pair(ots, login_id);
    }
    assert(response.result() == boost::beast::http::status::found);
    const auto location = response.at(boost::beast::http::field::location);
    co_return co_await connect(std::move(location));
}

boost::asio::awaitable<api_client::login_response> api_client::login(std::string path) {
    auto [ots, login_id] = co_await connect(path);
    auto token = client_.jar().get(ots);

    client_.set_default_headers({
        {"Origin", "https://demo.tradedirect365.com"},
        {
            "Referer",
            "https://demo.tradedirect365.com/Advanced.aspx?ots=" + ots
        },
    });
    co_return login_response{token.value, login_id};
}

boost::asio::awaitable<void> api_client::update_session_token() {
    static const char *path = "/UTSAPI.asmx/UpdateClientSessionID";
    static const headers hdrs = headers({
        {"Content-Type", "application/json; charset=utf-8"},
        {"X-Requested-With", "XMLHttpRequest"}
    });
    auto resp = co_await client_.post(path, hdrs);
    assert(resp.result() == boost::beast::http::status::ok);
    co_return;
}

boost::asio::awaitable<std::vector<market_group> >
api_client::get_market_super_group() {
    auto resp = co_await client_.post("/UTSAPI.asmx/GetMarketSuperGroup",
                                      "application/json", "");
    assert(resp.result() == boost::beast::http::status::ok);
    auto j = json::parse(resp.body());
    auto rv = j["d"].get<std::vector<market_group> >();
    co_return rv;
}

boost::asio::awaitable<std::vector<market_group> >
api_client::get_market_group(unsigned int id) {
    json body = {{"superGroupId", id}};
    auto resp =
            co_await client_.post("/UTSAPI.asmx/GetMarketGroup",
                                  "application/json; charset=utf-8", body.dump());
    assert(resp.result() == boost::beast::http::status::ok);
    auto j = json::parse(resp.body());
    auto rv = j["d"].get<std::vector<market_group> >();
    co_return rv;
}

boost::asio::awaitable<std::vector<market> >
api_client::get_market_quote(unsigned int id) {
    json body = {
        {"groupID", id}, {"keyword", ""}, {"popular", false},
        {"portfolio", false}, {"search", false},
    };
    auto resp = co_await client_.post("/UTSAPI.asmx/GetMarketQuote",
                                      "application/json", body.dump());
    assert(resp.result() == boost::beast::http::status::ok);
    auto j = json::parse(resp.body());
    auto rv = j["d"].get<std::vector<market> >();
    co_return rv;
}
