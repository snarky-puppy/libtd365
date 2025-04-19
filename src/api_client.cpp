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
#include <iostream>
#include <nlohmann/json.hpp>
#include <regex>

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http; // from <boost/beast/http.hpp>
namespace net = boost::asio; // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl; // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>
namespace net = boost::asio;
using json = nlohmann::json;

api_client::api_client(td_context_view ctx)
    : ctx_(ctx)
      , timer_(boost::asio::steady_timer(ctx_.executor)) {
}

boost::asio::awaitable<std::pair<std::string, std::string> > api_client::post_login_flow(std::string path) {
    auto response = co_await client_->get(path);
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
    co_return co_await post_login_flow(std::move(location));
}

boost::asio::awaitable<api_client::auth_info> api_client::open(std::string host, std::string path) {
    client_ = std::make_unique<http_client>(ctx_, host);
    auto [ots, login_id] = co_await post_login_flow(path);
    auto token = client_->jar().get(ots);

    client_->set_default_headers({
        {"Origin", "https://demo.tradedirect365.com"},
        {
            "Referer",
            "https://demo.tradedirect365.com/Advanced.aspx?ots=" + ots
        },
    });
    co_return auth_info{token.value, login_id};
}

boost::asio::awaitable<void> api_client::close() {
    timer_.cancel();
    co_return;
}

void api_client::start_session_loop(std::atomic<bool> &shutdown) {
    static const auto path = "/UTSAPI.asmx/UpdateClientSessionID";
    static const auto hdrs = headers({
        {"Content-Type", "application/json; charset=utf-8"},
        {"X-Requested-With", "XMLHttpRequest"}
    });

    auto loop = [this, &shutdown]() -> net::awaitable<void> {
        auto timeout = std::chrono::seconds(60);
        while (!shutdown) {
            try {
                if (auto resp = co_await client_->post(path, hdrs); resp.result() != http::status::ok) {
                    // can happen when using mitmproxy
                    timeout = std::chrono::seconds(0);
                } else {
                    timeout = std::chrono::seconds(60);
                    auto j = json::parse(resp.body());
                    if (j["d"]["Status"].get<int>() != 0) {
                        std::cerr << "TODO: log out\n";
                    }
                }
            } catch (const boost::system::error_code &ec) {
                if (ec == http::error::end_of_stream) {
                    timeout = std::chrono::seconds(0);
                } else {
                    std::cerr << "session_loop: " << ec.message() << std::endl;
                    throw ec;
                }
            }catch (...) {
                std::cerr << "session_loop: unknown error" << std::endl;
                throw std::current_exception();
            }

            timer_.expires_after(timeout);
            co_await timer_.async_wait();
        }
        std::cout << "api_client::session_loop exiting" << std::endl;
        co_return;
    };

    boost::asio::co_spawn(ctx_.executor, std::move(loop), boost::asio::detached);
}

boost::asio::awaitable<std::vector<market_group> >
api_client::get_market_super_group() {
    auto resp = co_await client_->post("/UTSAPI.asmx/GetMarketSuperGroup",
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
            co_await client_->post("/UTSAPI.asmx/GetMarketGroup",
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
    auto resp = co_await client_->post("/UTSAPI.asmx/GetMarketQuote",
                                       "application/json", body.dump());
    assert(resp.result() == boost::beast::http::status::ok);
    auto j = json::parse(resp.body());
    auto rv = j["d"].get<std::vector<market> >();
    co_return rv;
}
