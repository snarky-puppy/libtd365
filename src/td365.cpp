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
#include <spdlog/spdlog.h>

// struct cancellable {
//     void emit(net::cancellation_type ct = net::cancellation_type::all) {
//         std::lock_guard<std::mutex> _(mtx);
//
//         for (auto &sig : sigs)
//             sig.emit(ct);
//     }
//
//     auto use_awaitable() {
//         return boost::asio::bind_cancellation_slot(slot(),
//                                                    boost::asio::use_awaitable);
//     }
//
//     template <typename F> void spawn(boost::asio::any_io_executor ex, F &&f)
//     {
//         boost::asio::co_spawn(
//             ex, f,
//             boost::asio::bind_cancellation_slot(slot(),
//             boost::asio::detached));
//     }
//
//   protected:
//     std::list<net::cancellation_signal> sigs;
//     std::mutex mtx;
//
//     net::cancellation_slot slot() {
//         std::lock_guard<std::mutex> _(mtx);
//
//         auto itr = std::find_if(sigs.begin(), sigs.end(),
//                                 [](net::cancellation_signal &sig) {
//                                     return !sig.slot().has_handler();
//                                 });
//
//         if (itr != sigs.end()) {
//             return itr->slot();
//         }
//         return sigs.emplace_back().slot();
//     }
// };

namespace td365 {
namespace net = boost::asio; // from <boost/asio.hpp>

td365::td365(const user_callbacks &callbacks) : ws_client_(callbacks) {}

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
    // Reset the promise/future for a new connection attempt
    connected_ = false;
    std::promise<void> p;
    auto fut = p.get_future();

    net::co_spawn(
        io_context_,
        [&]() -> net::awaitable<void> {
            try {
                auto auth_detail = co_await auth_fn();
                auto [token, login_id] =
                    co_await rest_client_.connect(auth_detail.platform_url);

                while (!shutdown_) {
                    co_await ws_client_.connect(auth_detail.sock_host);
                    co_await ws_client_.message_loop(login_id, token,
                                                     shutdown_);
                }
                spdlog::info("message loop exiting");
            } catch (...) {
                std::println(std::cerr, "ws_client: unknown exception");
            }

            // rest_client_.start_session_loop();

            p.set_value();
            p.set_exception(std::current_exception());

            co_return;
        },
        net::detached);

    start_io_thread();
    fut.get();                  // wait for connect() to finish
    ws_client_.wait_for_auth(); // wait for websocket to finish authenticating
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
