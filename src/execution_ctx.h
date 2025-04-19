/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */


#ifndef EXECUTION_CTX_H
#define EXECUTION_CTX_H

#include <boost/asio.hpp>
#include <boost/asio/use_awaitable.hpp>

struct td_context_view {
    boost::asio::any_io_executor executor;
    boost::asio::cancellation_slot cancel_slot;
    std::atomic<bool> *shutdown_flag = nullptr;

    auto cancelable() const {
        return boost::asio::bind_cancellation_slot(cancel_slot, boost::asio::use_awaitable);
    }
};

struct td_context {
    boost::asio::io_context io;
    boost::asio::cancellation_signal cancel_signal;
    std::atomic<bool> shutdown{false};

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
            .shutdown_flag = &shutdown,
        };
    };
};

#endif //EXECUTION_CTX_H
