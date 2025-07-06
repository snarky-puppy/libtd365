/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#pragma once

#include "authenticator.h"
#include "rest_api.h"
#include "types.h"
#include "ws_client.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace td365 {

template <typename H>
concept UserCallbacksLike = requires(H h, tick &&t, account_summary &&a,
                                     account_details &&d, trade_details &&e) {
    { h.on_tick(std::move(t)) } -> std::same_as<void>;
    { h.on_account_summary(std::move(a)) } -> std::same_as<void>;
    { h.on_account_details(std::move(d)) } -> std::same_as<void>;
    { h.on_trade_established(std::move(e)) } -> std::same_as<void>;
};

class td365 {
  public:
    explicit td365();

    ~td365();

    user_callbacks &callbacks() { return callbacks_; }

    void connect(const std::string &username, const std::string &password,
                 const std::string &account_id);

    void connect();

    void subscribe(int quote_id);
    void unsubscribe(int quote_id);

    std::vector<market_group> get_market_super_group();
    std::vector<market_group> get_market_group(int id);
    std::vector<market> get_market_quote(int id);
    market_details_response get_market_details(int id);
    void trade(const trade_request &&request);
    std::vector<candle> backfill(int market_id, int quote_id, size_t sz,
                                 chart_duration dur);

  private:
    template <typename Awaitable>
    auto run_awaitable(Awaitable awaitable) -> typename Awaitable::value_type;

    auto run_awaitable(boost::asio::awaitable<void>) -> void;

    user_callbacks callbacks_;
    boost::asio::io_context io_context_;
    std::thread io_thread_;

    rest_api rest_client_;
    ws_client ws_client_;

    std::atomic<bool> shutdown_{false};

    std::promise<void> connect_p_;
    std::future<void> connect_f_;

    void connect(std::function<boost::asio::awaitable<web_detail>()> f);

    void start_io_thread();
};
} // namespace td365
