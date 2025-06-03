/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#pragma once

#include "types.h"

#include <boost/asio.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <list>
#include <mutex>
#include <spdlog/spdlog.h>

namespace net = boost::asio;

// A simple helper for cancellation_slot
struct cancellation_signals {
  std::list<net::cancellation_signal> sigs;
  std::mutex mtx;
  void emit(net::cancellation_type ct = net::cancellation_type::all) {
    std::lock_guard<std::mutex> _(mtx);

    for (auto &sig : sigs)
      sig.emit(ct);
  }

  net::cancellation_slot slot() {
    std::lock_guard<std::mutex> _(mtx);

    auto itr = std::find_if(sigs.begin(), sigs.end(),
                            [](net::cancellation_signal &sig) {
                              return !sig.slot().has_handler();
                            });

    if (itr != sigs.end())
      return itr->slot();
    else
      return sigs.emplace_back().slot();
  }
};

struct user_callbacks {
  using tick_callback = std::function<void(tick &&)>;
  using account_summary_callback = std::function<void(account_summary &&)>;
  using account_details_callback = std::function<void(account_details &&)>;

  tick_callback tick_cb = nullptr;
  account_summary_callback account_sum_cb = nullptr;
  account_details_callback account_detail_cb = nullptr;

  void on_tick(tick &&t) const {
    if (tick_cb) {
      tick_cb(std::move(t));
    }
  }

  void on_account_summary(account_summary &&a) const {
    if (account_sum_cb) {
      account_sum_cb(std::move(a));
    }
  }

  void on_account_details(account_details &&a) const {
    if (account_detail_cb) {
      account_detail_cb(std::move(a));
    }
  }
};

struct td_context_view {
  boost::asio::any_io_executor executor;
  boost::asio::cancellation_slot cancel_slot;
  std::atomic<bool> *shutdown_flag_ = nullptr;
  user_callbacks &usr_ctx;

  auto cancelable() const {
    return boost::asio::bind_cancellation_slot(cancel_slot,
                                               boost::asio::use_awaitable);
  }

  void cancel() { shutdown_flag_->store(true); }

  bool is_shutting_down() const { return shutdown_flag_->load(); }
};

struct td_context {
  td_context() = delete;

  td_context(td_context &&) = delete;

  td_context(const td_context &) = delete;

  td_context(user_callbacks &user_ctx) : usr_ctx{user_ctx} {}

  boost::asio::io_context io;
  boost::asio::cancellation_signal cancel_signal;
  std::atomic<bool> shutdown{false};
  user_callbacks &usr_ctx;

  auto executor() { return io.get_executor(); }

  void cancel() {
    shutdown = true;
    spdlog::info("Cancelling all tasks");
    cancel_signal.emit(boost::asio::cancellation_type::all);
  }

  boost::asio::cancellation_slot cancellation_slot() {
    return cancel_signal.slot();
  }

  bool is_shutdown() const { return shutdown.load(std::memory_order_relaxed); }

  auto use_awaitable() {
    return boost::asio::bind_cancellation_slot(cancellation_slot(),
                                               boost::asio::use_awaitable);
  }

  auto context() {
    auto ctx = td_context_view{
        .executor = io.get_executor(),
        .cancel_slot = cancel_signal.slot(),
        .shutdown_flag_ = &shutdown,
        .usr_ctx = usr_ctx,
    };
    ctx.cancel_slot.assign([](boost::asio::cancellation_type type) {
      spdlog::info("Cancellation triggered. Type = {}", static_cast<int>(type));
      // Or: std::cerr << "Cancelled with type: " << static_cast<int>(type) <<
      // "\n"; Set breakpoint here
    });
    return ctx;
  };
};
