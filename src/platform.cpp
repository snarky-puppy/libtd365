/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#include "platform.h"
#include "authenticator.h"
#include "utils.h"
#include "ws_client.h"
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/detached.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <iostream>

namespace net = boost::asio; // from <boost/asio.hpp>

platform::platform()
  : api_client_(ctx_.context())
    , ws_client_(ctx_.context(), [this](tick &&t) { on_tick_received(std::move(t)); })
    , connected_future_(connected_promise_.get_future()) {
}

platform::~platform() {
  if (io_thread_.joinable()) {
    io_thread_.join();
  }
  std::unique_lock<std::mutex> lock(tick_queue_mutex_);
  shutdown_ = true;
  tick_queue_cv_.notify_all();
}

void platform::connect() {
  connect([]() -> net::awaitable<account_detail> {
    return authenticator::authenticate();
  });
  start_io_thread();
  connected_future_.get();
}

void platform::connect(const std::string &username, const std::string &password,
                       const std::string &account_id) {
  connect([&, this]()-> net::awaitable<account_detail> {
    return authenticator::authenticate(ctx_.context(), username, password, account_id);
  });
  start_io_thread();
  connected_future_.get();
}

void platform::connect(std::function<net::awaitable<account_detail>()> f) {
  // Reset the promise/future for a new connection attempt
  connected_ = false;

  net::co_spawn(ctx_.executor(), [&, this]() -> net::awaitable<void> {
    try {
      auto auth_detail = co_await f();
      auto [token, login_id] = co_await api_client_.open(auth_detail.platform_url.host, auth_detail.platform_url.path);

      ws_client_.start_loop(std::move(auth_detail.sock_host), std::move(login_id), std::move(token));

      api_client_.start_session_loop(shutdown_);

      connected_ = true;
      connected_promise_.set_value();
    } catch (...) {
      connected_promise_.set_exception(std::current_exception());
    }

    co_return;
  }, net::detached);
}

void platform::start_io_thread() {
  if (io_thread_.joinable()) return;
  io_thread_ = std::thread([this]() {
    try {
      std::cout << __FILE__ << ":" << __LINE__ << std::endl;
      ctx_.io.run();
      std::cout << __FILE__ << ":" << __LINE__ << std::endl;
    } catch (const std::exception &e) {
      std::cerr << "io_thread: exception: " << e.what() << std::endl;
    } catch (...) {
      std::cerr << "io_thread: unknown exception" << std::endl;
    }
  });
}

void platform::subscribe(int quote_id) {
  run_awaitable(ws_client_.subscribe(quote_id));
}

void platform::unsubscribe(int quote_id) {
  run_awaitable(ws_client_.unsubscribe(quote_id));
}

std::vector<market_group> platform::get_market_super_group() {
  return run_awaitable(api_client_.get_market_super_group());
}

std::vector<market_group> platform::get_market_group(int id) {
  return run_awaitable(api_client_.get_market_group(id));
}

std::vector<market> platform::get_market_quote(int id) {
  return run_awaitable(api_client_.get_market_quote(id));
}

void platform::on_tick_received(tick &&t) {
  auto lock = std::unique_lock(tick_queue_mutex_);
  tick_queue_.push(std::move(t));
  tick_queue_cv_.notify_one();
}

void platform::main_loop(std::function<void(tick &&)> tick_callback) {
  while (!shutdown_) {
    std::vector<tick> ticks_to_process;

    // Get ticks from the queue
    {
      std::unique_lock<std::mutex> lock(tick_queue_mutex_);
      if (tick_queue_.empty()) {
        // Wait for new ticks or shutdown
        tick_queue_cv_.wait(
          lock, [this] { return !tick_queue_.empty() || shutdown_; });
      }

      // If we're shutting down, exit
      if (shutdown_ && tick_queue_.empty()) {
        return;
      }

      // Process all available ticks
      while (!tick_queue_.empty()) {
        ticks_to_process.push_back(tick_queue_.front());
        tick_queue_.pop();
      }
    }

    // Call user callback for each tick (outside the lock)
    for (auto &&tick: ticks_to_process) {
      tick_callback(std::move(tick));
    }
  }
}

template<typename Awaitable>
auto platform::run_awaitable(Awaitable awaitable) ->
  typename Awaitable::value_type {
  if (ctx_.io.stopped()) throw std::runtime_error("io_context is stopped");

  std::promise<typename Awaitable::value_type> promise;
  auto future = promise.get_future();

  net::co_spawn(
    ctx_.executor(),
    [awaitable = std::move(awaitable),
      &promise]() mutable -> net::awaitable<void> {
      try {
        auto result = co_await std::move(awaitable);
        promise.set_value(std::move(result));
      } catch (const std::exception &e) {
        std::cerr << "run_awaitable: " << e.what() << std::endl;
        promise.set_exception(std::current_exception());
      } catch (const boost::system::error_code &e) {
        std::cerr << "run_awaitable: " << e.what() << std::endl;
        promise.set_exception(std::current_exception());
      } catch (...) {
        std::cerr << "run_awaitable: unexpected exception" << std::endl;
        promise.set_exception(std::current_exception());
      }
    },
    net::detached);

  return future.get();
}

auto platform::run_awaitable(net::awaitable<void> awaitable) -> void {
  if (ctx_.io.stopped()) throw std::runtime_error("io_context is stopped");

  std::promise<void> promise;
  auto future = promise.get_future();

  net::co_spawn(
    ctx_.executor(),
    [awaitable = std::move(awaitable), &promise]() mutable -> net::awaitable<void> {
      try {
        co_await std::move(awaitable);
        promise.set_value();
      } catch (const std::exception &e) {
        std::cerr << "run_awaitable<void>: " << e.what() << std::endl;
        promise.set_exception(std::current_exception());
      } catch (const boost::system::error_code &e) {
        std::cerr << "run_awaitable<void>: " << e.what() << std::endl;
        promise.set_exception(std::current_exception());
      } catch (...) {
        std::cerr << "run_awaitable<void>: unexpected exception" << std::endl;
        promise.set_exception(std::current_exception());
      }
    },
    net::detached);

  future.get();
}
