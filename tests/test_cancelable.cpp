/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#include "execution_ctx.h"

#include <boost/asio.hpp>
#include <catch.hpp>

using namespace std::chrono_literals;

struct actor : cancellable {
    boost::asio::awaitable<void> do_work() {
        auto ex = co_await boost::asio::this_coro::executor;
        boost::asio::steady_timer timer(ex);
        timer.expires_after(1s);
        co_await timer.async_wait(use_awaitable());
        co_return;
    }
};

TEST_CASE("cancellable", "[cancelable]") {
    boost::asio::io_context ioc;

    actor a, b, c;

    ioc.run();
}
