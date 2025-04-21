/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */


#ifndef EXECUTION_CTX_H
#define EXECUTION_CTX_H

#include "types.h"

#include <boost/asio.hpp>
#include <boost/asio/use_awaitable.hpp>

struct td_user_context {
    boost::asio::any_io_executor executor;
    using tick_callback = std::function<void(tick &&)>;
    using account_summary_callback = std::function<void(account_summary &&)>;
    using account_details_callback = std::function<void(account_details &&)>;

    tick_callback tick_cb = nullptr;
    account_summary_callback account_sum_cb = nullptr;
    account_details_callback account_detail_cb = nullptr;

    void on_tick(tick &&t) {
        if (tick_cb) {
            boost::asio::post(executor,
                              [callback = tick_cb, data = std::move(t)]() mutable {
                                  callback(std::move(data));
                              });
        }
    }

    void on_account_summary(account_summary &&a) {
        if (account_sum_cb) {
            boost::asio::post(executor,
                              [callback = account_sum_cb, data = std::move(a)]() mutable {
                                  callback(std::move(data));
                              });
        }
    }

    void on_account_details(account_details &&a) {
        if (account_detail_cb) {
            boost::asio::post(executor,
                              [callback = account_detail_cb, data = std::move(a)]() mutable {
                                  callback(std::move(data));
                              });
        }
    }
};

struct td_context_view {
    boost::asio::any_io_executor executor;
    boost::asio::cancellation_slot cancel_slot;
    std::atomic<bool> *shutdown_flag_ = nullptr;
    td_user_context &usr_ctx;

    auto cancelable() const {
        return boost::asio::use_awaitable;
        // return boost::asio::bind_cancellation_slot(cancel_slot, boost::asio::use_awaitable);
    }

    void cancel() {
        shutdown_flag_->store(true);
    }

    bool is_shutting_down() const {
        return shutdown_flag_->load();
    }
};

struct td_context {
    td_context() = delete;

    td_context(td_context &&) = delete;

    td_context(const td_context &) = delete;

    td_context(td_user_context &user_ctx) : usr_ctx{user_ctx} {
    }

    boost::asio::io_context io;
    boost::asio::cancellation_signal cancel_signal;
    std::atomic<bool> shutdown{false};
    td_user_context &usr_ctx;

    auto executor() { return io.get_executor(); }

    void cancel() {
        shutdown = true;
        cancel_signal.emit(boost::asio::cancellation_type::all);
    }

    boost::asio::cancellation_slot cancellation_slot() {
        return cancel_signal.slot();
    }

    bool is_shutdown() const {
        return shutdown.load(std::memory_order_relaxed);
    }

    auto use_awaitable() {
        return boost::asio::bind_cancellation_slot(cancellation_slot(), boost::asio::use_awaitable);
    }

    auto context() {
        return td_context_view{
            .executor = io.get_executor(),
            .cancel_slot = cancel_signal.slot(),
            .shutdown_flag_ = &shutdown,
            .usr_ctx = usr_ctx,
        };
    };
};

#endif //EXECUTION_CTX_H
