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
#include <boost/exception/diagnostic_information.hpp>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

platform::platform() {
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
  } catch (const boost::exception &e) {
    std::string diag = boost::diagnostic_information(e);
    std::cerr << "dtor: " << diag << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "dtor: " << e.what() << std::endl;
  }
}

void platform::connect_demo() {
  connect(authenticator::authenticate());
}

void platform::connect(const std::string &username, const std::string &password,
                       const std::string &account_id) {
  connect(authenticator::authenticate(username, password, account_id));
}

void platform::connect(account_detail auth_detail) {
  try {
    api_client_ = std::make_unique<api_client>(auth_detail.platform_url.host);
    auto token = api_client_->login(auth_detail.platform_url.path);

    ws_client_ = std::make_unique<ws_client>(
      shutdown_,
      [this](const tick &t) { on_tick_received(t); });

    ws_client_->start_loop(std::move(auth_detail.sock_host), std::move(auth_detail.login_id), std::move(token));
  } catch (const boost::exception &e) {
    std::string diag = boost::diagnostic_information(e);
    std::cerr << "connect: " << diag << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "connect: " << e.what() << std::endl;
  }
}


void platform::shutdown() {
  shutdown_ = true;
  ws_client_->close_sync();
}

void platform::subscribe(int quote_id) {
  ws_client_->subscribe_sync(quote_id);
}

void platform::unsubscribe(int quote_id) {
  ws_client_->unsubscribe_sync(quote_id);
}

std::vector<market_group> platform::get_market_super_group() {
  if (!api_client_) {
    return {};
  }
  return api_client_->get_market_super_group();
}

std::vector<market_group> platform::get_market_group(int id) {
  if (!api_client_) {
    return {};
  }
  return api_client_->get_market_group(id);
}

std::vector<market> platform::get_market_quote(int id) {
  if (!api_client_) {
    return {};
  }
  return api_client_->get_market_quote(id);
}

void platform::on_tick_received(const tick &t) {
  std::unique_lock<std::mutex> lock(tick_queue_mutex_);
  tick_queue_.push(t);
  tick_queue_cv_.notify_one();
}

void platform::main_loop(std::function<void(const tick &)> tick_callback) {
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
      tick_callback(tick);
    }
  }
}
