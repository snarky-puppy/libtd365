#include <boost/asio.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <iostream>
#include <spdlog/spdlog.h>

namespace asio = boost::asio;
using asio::use_awaitable;

asio::awaitable<void> run_timer(const std::string_view name,
                                asio::cancellation_slot slot) {
  auto ex = co_await asio::this_coro::executor;
  asio::steady_timer timer(ex, std::chrono::seconds{5});

  boost::system::error_code ec;
  auto ct =
      asio::bind_cancellation_slot(slot, redirect_error(use_awaitable, ec));

  spdlog::info("{} starting wait", name);

  co_await timer.async_wait(ct);

  if (ec == asio::error::operation_aborted)
    spdlog::info("{} aborted", name);
  else if (ec)
    spdlog::info("{} failed: {}", name, ec.message());
  else
    spdlog::info("{} completed", name);
}

int main() {
  asio::io_context ctx;

  // 1) Prepare a cancellation_signal
  asio::cancellation_signal cancel_signal;

  // 2) Spawn three instances of our timer coroutine
  asio::co_spawn(ctx, run_timer("fn1", cancel_signal.slot()), asio::detached);
  asio::co_spawn(ctx, run_timer("fn2", cancel_signal.slot()), asio::detached);
  asio::co_spawn(ctx, run_timer("fn3", cancel_signal.slot()), asio::detached);

  // 3) Schedule cancellation *within* the same io_context after 2s
  asio::steady_timer cancel_timer(ctx, std::chrono::seconds{2});
  cancel_timer.async_wait([&](auto) {
    spdlog::info("Cancelling all tasks");
    cancel_signal.emit(asio::cancellation_type::all);
  });

  // 4) Run everything!
  ctx.run();
}
