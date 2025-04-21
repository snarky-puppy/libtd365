/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#ifndef TD365_H
#define TD365_H

#include "authenticator.h"
#include "api_client.h"
#include "ws_client.h"

#include <functional>
#include <string>
#include <vector>

class td365 : public std::enable_shared_from_this<td365> {
public:
    explicit td365(td_user_context &client_ctx);

    ~td365();

    void connect(const std::string &username, const std::string &password,
                 const std::string &account_id);

    void connect();

    void shutdown();

    void subscribe(int quote_id);

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

    std::promise<void> connected_promise_;
    std::future<void> connected_future_;
    std::atomic<bool> connected_{false};

    std::atomic<bool> shutdown_{false};
    boost::asio::cancellation_signal cancel_signal;

    void connect(std::function<boost::asio::awaitable<web_detail>()> f);

    void start_io_thread();
};

#endif // TD365_H
