/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include <authenticator.h>
#include <condition_variable>
#include <queue>
#include <thread>

#include "api_client.h"
#include "td365.h"
#include "ws_client.h"
#include "execution_ctx.h"

class ws_client;
class api_client;

class platform : public std::enable_shared_from_this<platform> {
public:
    explicit platform();

    ~platform();

    void connect(const std::string &username, const std::string &password,
                 const std::string &account_id);

    void connect();

    void shutdown();

    void subscribe(int quote_id);

    void main_loop(std::function<void(tick &&)> tick_callback);

    void unsubscribe(int quote_id);

    std::vector<market_group> get_market_super_group();

    std::vector<market_group> get_market_group(int id);

    std::vector<market> get_market_quote(int id);

private:
    template<typename Awaitable>
    auto run_awaitable(Awaitable awaitable) -> typename Awaitable::value_type;

    auto run_awaitable(boost::asio::awaitable<void>) -> void;

    td_context ctx_;
    std::thread io_thread_;

    api_client api_client_;
    ws_client ws_client_;

    // Thread-safe queue for ticks
    std::queue<tick> tick_queue_;
    std::mutex tick_queue_mutex_;
    std::condition_variable tick_queue_cv_;

    std::promise<void> connected_promise_;
    std::future<void> connected_future_;
    std::atomic<bool> connected_{false};

    std::atomic<bool> shutdown_{false};
    boost::asio::cancellation_signal cancel_signal;


    void connect(std::function<boost::asio::awaitable<account_detail>()> f);

    void on_tick_received(tick &&t);

    void process_ticks_thread();

    void start_io_thread();
};

#endif // PLATFORM_H
