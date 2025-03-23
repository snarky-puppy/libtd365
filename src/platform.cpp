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
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <iostream>
#include <nlohmann/json.hpp>

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http; // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio; // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl; // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>
using net::use_awaitable;
using json = nlohmann::json;

template<typename Duration>
boost::asio::awaitable<void> async_sleep(Duration duration) {
  boost::asio::steady_timer timer(co_await boost::asio::this_coro::executor,
                                  duration);
  co_await timer.async_wait(boost::asio::use_awaitable);
}

void confirm_login_response(const nlohmann::json &) {
  // TODO: think of something to do something here
}

platform::platform(std::function<void(const tick &)> tick_callback)
  : work_guard_(io_context_.get_executor()),
    ws_client_(io_context_.get_executor(), shutdown_,
               [this](const tick &t) { on_tick_received(t); }),
    user_tick_callback_(std::move(tick_callback)) {
  // Start the IO thread
  io_thread_ = std::thread([this]() {
    try {
      ssl_ctx.set_default_verify_paths();
      io_context_.run();
    } catch (const std::exception &e) {
      std::cerr << "IO thread exception: " << e.what() << std::endl;
    }
  });

  // Start the tick processing thread
  if (user_tick_callback_) {
    tick_processing_thread_ = std::thread([this]() { process_ticks_thread(); });
  }
}

void platform::connect_demo() {
  try {
    authenticator auther(username, password, account_id);
    auto auth_detail =
        run_awaitable(auther.authenticate(io_context_.get_executor()));
    api_client_ = std::make_unique<api_client>(io_context_.get_executor(),
                                               auth_detail.platform_url.host);
    auto token =
        run_awaitable(api_client_->login(auth_detail.platform_url.path));

    ws_client_.start_loop(std::move(auth_detail.sock_host), std::move(auth_detail.login_id), std::move(token));

    // this doesn't seem to set any cookie or fetch any token
    // boost::asio::co_spawn(io_context_.get_executor(), [self]() ->
    // boost::asio::awaitable<void> { co_await
    // api_client_->update_session_token();
    // }, boost::asio::detached);
  } catch (const boost::exception &e) {
    std::string diag = boost::diagnostic_information(e);
    std::cerr << "connect: " << diag << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "connect: " << e.what() << std::endl;
  }
}

void platform::connect(const std::string &username, const std::string &password,
                       const std::string &account_id) {
  try {
    authenticator auther(username, password, account_id);
    auto auth_detail =
        run_awaitable(auther.authenticate(io_context_.get_executor()));
    api_client_ = std::make_unique<api_client>(io_context_.get_executor(),
                                               auth_detail.platform_url.host);
    auto token =
        run_awaitable(api_client_->login(auth_detail.platform_url.path));

    ws_client_.start_loop(std::move(auth_detail.sock_host), std::move(auth_detail.login_id), std::move(token));

    // this doesn't seem to set any cookie or fetch any token
    // boost::asio::co_spawn(io_context_.get_executor(), [self]() ->
    // boost::asio::awaitable<void> { co_await
    // api_client_->update_session_token();
    // }, boost::asio::detached);
  } catch (const boost::exception &e) {
    std::string diag = boost::diagnostic_information(e);
    std::cerr << "connect: " << diag << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "connect: " << e.what() << std::endl;
  }
}

platform::~platform() {
  try {
    shutdown();

    // Signal the tick processing thread to exit
    {
      std::unique_lock<std::mutex> lock(tick_queue_mutex_);
      shutdown_ = true;
      tick_queue_cv_.notify_all();
    }

    // Join the tick processing thread if it's running
    if (tick_processing_thread_.joinable()) {
      tick_processing_thread_.join();
    }

    // Now release the work guard to allow the io_context to stop
    work_guard_.reset();
    io_context_.stop();
    io_thread_.join();
  } catch (const boost::exception &e) {
    std::string diag = boost::diagnostic_information(e);
    std::cerr << "dtor: " << diag << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "dtor: " << e.what() << std::endl;
  }
}

void platform::shutdown() { shutdown_ = true; }

void platform::subscribe(int quote_id) {
  run_awaitable(ws_client_.subscribe(quote_id));
}

void platform::wait_for_disconnect() { ws_client_.wait_for_disconnect(); }

void platform::unsubscribe(int quote_id) {
  run_awaitable(ws_client_.unsubscribe(quote_id));
}

std::vector<market_group> platform::get_market_super_group() {
  return run_awaitable(api_client_->get_market_super_group());
}

std::vector<market_group> platform::get_market_group(int id) {
  return run_awaitable(api_client_->get_market_group(id));
}

std::vector<market> platform::get_market_quote(int id) {
  return run_awaitable(api_client_->get_market_quote(id));
}

void platform::on_tick_received(const tick &t) {
  // This method is called by the WS client (on the IO thread)
  if (user_tick_callback_) {
    // Add the tick to the queue for the processing thread
    std::unique_lock<std::mutex> lock(tick_queue_mutex_);
    tick_queue_.push(t);
    tick_queue_cv_.notify_one();
  }
}

void platform::process_ticks_thread() {
  // This method runs on the tick processing thread
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
        break;
      }

      // Process all available ticks
      while (!tick_queue_.empty()) {
        ticks_to_process.push_back(tick_queue_.front());
        tick_queue_.pop();
      }
    }

    // Call user callback for each tick (outside the lock)
    for (const auto &tick: ticks_to_process) {
      try {
        if (user_tick_callback_) {
          user_tick_callback_(tick);
        }
      } catch (const std::exception &e) {
        // Log exception but don't let user code crash our thread
        std::cerr << "Error in user tick callback: " << e.what() << std::endl;
      }
    }
  }
}

template<typename Awaitable>
auto platform::run_awaitable(Awaitable awaitable) ->
  typename Awaitable::value_type {
  std::promise<typename Awaitable::value_type> promise;
  auto future = promise.get_future();

  net::co_spawn(
    io_context_.get_executor(),
    [awaitable = std::move(awaitable),
      &promise]() mutable -> net::awaitable<void> {
      try {
        auto result = co_await std::move(awaitable);
        promise.set_value(std::move(result));
      } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        promise.set_exception(std::current_exception());
      }
    },
    net::detached);

  return future.get();
}

auto platform::run_awaitable(net::awaitable<void> awaitable) -> void {
  std::promise<void> promise;
  auto future = promise.get_future();

  net::co_spawn(
    io_context_.get_executor(),
    [awaitable = std::move(awaitable),
      &promise]() mutable -> net::awaitable<void> {
      try {
        co_await std::move(awaitable);
        promise.set_value();
      } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        promise.set_exception(std::current_exception());
      }
    },
    net::detached);

  future.get();
}
