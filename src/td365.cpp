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

namespace td365 {
namespace net = boost::asio; // from <boost/asio.hpp>

td365::td365(const user_callbacks &callbacks)
    : ws_client_(callbacks),
      connected_future_(connected_promise_.get_future()) {}

td365::~td365() {
  if (io_thread_.joinable()) {
    io_thread_.join();
  }
}

void td365::connect() {
  connect([]() -> net::awaitable<web_detail> {
    return authenticator::authenticate();
  });
  start_io_thread();
  connected_future_.get();    // wait for connect() to finish
  ws_client_.wait_for_auth(); // wait for websocket to finish authenticating
}

void td365::connect(const std::string &username, const std::string &password,
                    const std::string &account_id) {
  connect([&, this]() -> net::awaitable<web_detail> {
    return authenticator::authenticate(username, password, account_id);
  });
  start_io_thread();
  connected_future_.get();    // wait for connect() to finish
  ws_client_.wait_for_auth(); // wait for websocket to finish authenticating
}

void td365::connect(std::function<net::awaitable<web_detail>()> f) {
  // Reset the promise/future for a new connection attempt
  connected_ = false;

  net::co_spawn(
      io_context_,
      [&, self = shared_from_this()]() -> net::awaitable<void> {
        try {
          auto auth_detail = co_await f();
          auto [token, login_id] =
              co_await api_client_->open(auth_detail.platform_url);

          ws_client_.start_loop(std::move(auth_detail.sock_host),
                                std::move(login_id), std::move(token));

          // api_client_.start_session_loop();

          connected_ = true;
          connected_promise_.set_value();
        } catch (...) {
          connected_promise_.set_exception(std::current_exception());
        }

        co_return;
      },
      net::detached);
}

void td365::start_io_thread() {
  if (io_thread_.joinable())
    return;
  io_thread_ = std::thread([this]() {
    try {
      ctx_.io.run();
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
  return run_awaitable(api_client_->get_market_super_group());
}

std::vector<market_group> td365::get_market_group(int id) {
  return run_awaitable(api_client_->get_market_group(id));
}

std::vector<market> td365::get_market_quote(int id) {
  return run_awaitable(api_client_->get_market_quote(id));
}

template <typename Awaitable>
auto td365::run_awaitable(Awaitable awaitable) ->
    typename Awaitable::value_type {
  if (io_context_.stopped())
    throw std::runtime_error("io_context is stopped");

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

// auto td365::run_awaitable(net::awaitable<void> awaitable) -> void {
//   if (ctx_.io.stopped())
//     throw std::runtime_error("io_context is stopped");
//
//   std::promise<void> promise;
//   auto future = promise.get_future();
//
//   net::co_spawn(
//       ctx_.executor(),
//       [awaitable = std::move(awaitable),
//        &promise]() mutable -> net::awaitable<void> {
//         try {
//           co_await std::move(awaitable);
//           promise.set_value();
//         } catch (...) {
//           promise.set_exception(std::current_exception());
//         }
//       },
//       net::detached);
//
//   future.get();
// }
} // namespace td365
