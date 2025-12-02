/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/detached.hpp>
#include <iostream>
#include <print>
#include <spdlog/spdlog.h>
#include <td365/authenticator.h>
#include <td365/td365.h>
#include <td365/ws_client.h>

namespace td365 {
namespace net = boost::asio; // from <boost/asio.hpp>

td365::td365() = default;

td365::~td365() = default;

void td365::connect() {
    net::io_context io_ctx;
    std::promise<web_detail> auth_promise;
    auto auth_future = auth_promise.get_future();

    net::co_spawn(
        io_ctx,
        [&]() -> net::awaitable<void> {
            try {
                auto detail = co_await authenticator::authenticate();
                auth_promise.set_value(std::move(detail));
            } catch (...) {
                auth_promise.set_exception(std::current_exception());
            }
        },
        net::detached);

    io_ctx.run();
    auto auth_detail = auth_future.get();

    std::promise<rest_api::auth_info> connect_promise;
    auto connect_future = connect_promise.get_future();

    net::co_spawn(
        io_ctx,
        [&]() -> net::awaitable<void> {
            try {
                auto result = co_await rest_client_.connect(auth_detail.platform_url);
                connect_promise.set_value(result);
            } catch (...) {
                connect_promise.set_exception(std::current_exception());
            }
        },
        net::detached);

    io_ctx.restart();
    io_ctx.run();
    auto auth_info = connect_future.get();

    ws_client_.connect(auth_detail.sock_host, auth_info.login_id, auth_info.token);
}

void td365::connect(const std::string &username, const std::string &password,
                    const std::string &account_id) {
    net::io_context io_ctx;
    std::promise<web_detail> auth_promise;
    auto auth_future = auth_promise.get_future();

    net::co_spawn(
        io_ctx,
        [&]() -> net::awaitable<void> {
            try {
                auto detail = co_await authenticator::authenticate(username, password, account_id);
                auth_promise.set_value(std::move(detail));
            } catch (...) {
                auth_promise.set_exception(std::current_exception());
            }
        },
        net::detached);

    io_ctx.run();
    auto auth_detail = auth_future.get();

    std::promise<rest_api::auth_info> connect_promise;
    auto connect_future = connect_promise.get_future();

    net::co_spawn(
        io_ctx,
        [&]() -> net::awaitable<void> {
            try {
                auto result = co_await rest_client_.connect(auth_detail.platform_url);
                connect_promise.set_value(result);
            } catch (...) {
                connect_promise.set_exception(std::current_exception());
            }
        },
        net::detached);

    io_ctx.restart();
    io_ctx.run();
    auto auth_info = connect_future.get();

    ws_client_.connect(auth_detail.sock_host, auth_info.login_id, auth_info.token);
}

void td365::subscribe(int quote_id) {
    ws_client_.subscribe(quote_id);
}

void td365::unsubscribe(int quote_id) {
    ws_client_.unsubscribe(quote_id);
}

event td365::wait(std::optional<std::chrono::milliseconds> timeout) {
    return ws_client_.read_and_process_message(timeout);
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

trade_response td365::trade(const trade_request &&request) {
    net::io_context io_ctx;
    std::promise<trade_response> promise;
    auto future = promise.get_future();

    net::co_spawn(
        io_ctx,
        [&]() -> net::awaitable<void> {
            try {
                co_await rest_client_.get_market_details(request.market_id);
                co_await rest_client_.sim_trade(request);
                auto response = co_await rest_client_.trade(std::move(request));
                promise.set_value(std::move(response));
            } catch (...) {
                promise.set_exception(std::current_exception());
            }
        },
        net::detached);

    io_ctx.run();
    return future.get();
}

std::vector<candle> td365::backfill(int market_id, int quote_id, size_t sz,
                                    chart_duration dur) {
    return run_awaitable(rest_client_.backfill(market_id, quote_id, sz, dur));
}

template <typename Awaitable>
auto td365::run_awaitable(Awaitable awaitable) ->
    typename Awaitable::value_type {

    net::io_context io_ctx;
    std::promise<typename Awaitable::value_type> promise;
    auto future = promise.get_future();

    net::co_spawn(
        io_ctx,
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

    io_ctx.run();
    return future.get();
}

auto td365::run_awaitable(net::awaitable<void> awaitable) -> void {
    net::io_context io_ctx;
    std::promise<void> promise;
    auto future = promise.get_future();

    net::co_spawn(
        io_ctx,
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

    io_ctx.run();
    future.get();
}
} // namespace td365
