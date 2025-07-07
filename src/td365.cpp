/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#include "td365.h"

#include "authenticator.h"
#include "ws_client.h"

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/detached.hpp>
#include <iostream>
#include <print>
#include <spdlog/spdlog.h>

namespace td365 {
namespace net = boost::asio; // from <boost/asio.hpp>

td365::td365() : ws_client_(callbacks_), connect_f_(connect_p_.get_future()) {}

td365::~td365() {
    if (io_thread_.joinable()) {
        io_thread_.join();
    }
}

void td365::connect() {
    connect([]() -> net::awaitable<web_detail> {
        return authenticator::authenticate();
    });
}

void td365::connect(const std::string &username, const std::string &password,
                    const std::string &account_id) {
    connect([&]() -> net::awaitable<web_detail> {
        return authenticator::authenticate(username, password, account_id);
    });
}

void td365::connect(std::function<net::awaitable<web_detail>()> auth_fn) {
    net::co_spawn(
        io_context_,
        [&]() -> net::awaitable<void> {
            try {
                auto auth_detail = co_await auth_fn();
                auto [token, login_id] =
                    co_await rest_client_.connect(auth_detail.platform_url);

                connect_p_.set_value();
                while (!shutdown_) {
                    co_await ws_client_.connect(auth_detail.sock_host);
                    co_await ws_client_.message_loop(login_id, token,
                                                     shutdown_);
                }
                spdlog::info("message loop exiting");
            } catch (const std::exception &e) {
                spdlog::error("ws_client: {}", e.what());
            } catch (...) {
                std::println(std::cerr, "ws_client: unknown exception");
                connect_p_.set_exception(std::current_exception());
                co_return;
            }

            co_return;
        },
        net::detached);

    start_io_thread();
    connect_f_.get();
    ws_client_.wait_for_auth();
}

void td365::start_io_thread() {
    if (io_thread_.joinable())
        return;
    io_thread_ = std::thread([this]() {
        try {
            io_context_.run();
            std::cerr << "io_thread: joining" << std::endl;
        } catch (const std::exception &e) {
            std::cerr << "io_thread: exception: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "io_thread: unknown exception" << std::endl;
        }
    });
}

void td365::subscribe(int quote_id) {
    run_awaitable(ws_client_.subscribe(quote_id));
}

void td365::unsubscribe(int quote_id) {
    run_awaitable(ws_client_.unsubscribe(quote_id));
}

std::vector<market_group> td365::get_market_super_group() {
    return run_awaitable(rest_client_.get_market_super_group());
}

std::vector<market_group> td365::get_market_group(int id) {
    return run_awaitable(rest_client_.get_market_group(id));
}

std::vector<market> td365::get_market_quote(int id) {
    return run_awaitable(rest_client_.get_market_quote(id));
}

market_details_response td365::get_market_details(int id) {
    return run_awaitable((rest_client_.get_market_details(id)));
}
void td365::trade(const trade_request &&request) {
    net::co_spawn(
        io_context_,
        [this, request = std::move(request)]() -> net::awaitable<void> {
            try {
                co_await rest_client_.get_market_details(request.market_id);
                co_await rest_client_.sim_trade(request);
                auto response = co_await rest_client_.trade(std::move(request));
                callbacks_.trade_response_cb(std::move(response));
            } catch (const std::exception &e) {
                spdlog::error("trade exception: {}", e.what());
            } catch (...) {
                std::println(std::cerr, "trade: unknown exception");
            }

            co_return;
        },
        net::detached);
}

std::vector<candle> td365::backfill(int market_id, int quote_id, size_t sz,
                                    chart_duration dur) {
    return run_awaitable(rest_client_.backfill(market_id, quote_id, sz, dur));
}

template <typename Awaitable>
auto td365::run_awaitable(Awaitable awaitable) ->
    typename Awaitable::value_type {

    std::promise<typename Awaitable::value_type> promise;
    auto future = promise.get_future();

    net::co_spawn(
        io_context_,
        [awaitable = std::move(awaitable),
         &promise]() mutable -> net::awaitable<void> {
            try {
                auto result = co_await std::move(awaitable);
                promise.set_value(std::move(result));
            } catch (...) {
                promise.set_exception(std::current_exception());
            }
        },
        net::detached);

    return future.get();
}

auto td365::run_awaitable(net::awaitable<void> awaitable) -> void {
    std::promise<void> promise;
    auto future = promise.get_future();

    net::co_spawn(
        io_context_,
        [awaitable = std::move(awaitable),
         &promise]() mutable -> net::awaitable<void> {
            try {
                co_await std::move(awaitable);
                promise.set_value();
            } catch (...) {
                promise.set_exception(std::current_exception());
            }
        },
        net::detached);

    future.get();
}
} // namespace td365
