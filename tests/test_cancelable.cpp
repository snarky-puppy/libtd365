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

TEST_CASE("steady_timer is cancelled via td_context::cancel()",
          "[cancelable]") {
  user_callbacks usr;
  td_context ctx{usr};

  // Spawn a coroutine that waits 1 second but is using your cancelable adaptor
  auto fut = boost::asio::co_spawn(
      ctx.io,
      [&ctx]() mutable -> boost::asio::awaitable<void> {
        // get executor from coroutine
        auto ex = co_await boost::asio::this_coro::executor;
        boost::asio::steady_timer timer(ex);
        timer.expires_after(1s);

        // this is your cancelable awaitable
        co_await timer.async_wait(ctx.use_awaitable());

        // if we get here, cancellation didn’t work
        FAIL("Timer completed instead of being cancelled");
        co_return;
      },
      boost::asio::use_future);

  // trigger cancellation before we run the I/O loop
  // schedule the cancel *after* the coroutine has started
  boost::asio::post(ctx.io.get_executor(), [&ctx] { ctx.cancel(); });

  // run the I/O loop so the timer callback (and cancellation) fire
  ctx.io.run();

  // The future.get() should propagate the operation_aborted exception
  bool cancelled = false;
  try {
    fut.get();
    FAIL("Expected boost::system::system_error");
  } catch (const boost::system::system_error &e) {
    if (e.code() == boost::asio::error::operation_aborted) {
      cancelled = true;
    }
  }
  REQUIRE(cancelled);
}

TEST_CASE("both steady_timers are cancelled via td_context::cancel()",
          "[cancelable]") {
  user_callbacks usr;
  td_context ctx{usr};

  auto fut = boost::asio::co_spawn(
      ctx.io,
      [&ctx]() mutable -> boost::asio::awaitable<void> {
        auto ex = co_await boost::asio::this_coro::executor;

        boost::asio::steady_timer timer1(ex);
        boost::asio::steady_timer timer2(ex);

        timer1.expires_after(2s);
        timer2.expires_after(2s);

        // Await both timers using the cancelable context
        boost::system::error_code ec1, ec2;
        try {
          co_await timer1.async_wait(ctx.use_awaitable());
          FAIL("Timer1 completed instead of being cancelled");
        } catch (const boost::system::system_error &e) {
          ec1 = e.code();
        }
        REQUIRE(ec1 == boost::asio::error::operation_aborted);

        try {
          co_await timer2.async_wait(ctx.use_awaitable());
          FAIL("Timer2 completed instead of being cancelled");
        } catch (const boost::system::system_error &e) {
          ec2 = e.code();
        }
        REQUIRE(ec2 == boost::asio::error::operation_aborted);

        co_return;
      },
      boost::asio::use_future);

  // Post cancellation after coroutine starts
  boost::asio::post(ctx.io.get_executor(), [&ctx] { ctx.cancel(); });

  ctx.io.run();

  // If we get here without exception, the test should pass
  bool cancelled = false;
  try {
    fut.get();
    cancelled = true;
  } catch (const std::runtime_error &e) {
    spdlog::error("Unexpected system_error thrown: {}", e.what());
    FAIL("Unexpected system_error thrown");
  } catch (...) {
    FAIL("Unexpected exception thrown");
  }

  REQUIRE(cancelled);
}

TEST_CASE("cancellation", "[api]") {
  user_callbacks usr;
  td_context ctx{usr};

  boost::asio::associated_cancellation_slot_t s =
      boost::asio::get_associated_cancellation_slot(ctx.context().cancelable());
}
