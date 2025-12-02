/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <td365/authenticator.h>
#include <td365/rest_api.h>
#include <td365/types.h>
#include <td365/ws_client.h>
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

    void connect(const std::string &username, const std::string &password,
                 const std::string &account_id);

    void connect();

    void subscribe(int quote_id);
    void unsubscribe(int quote_id);

    event wait(std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    std::vector<market_group> get_market_super_group();
    std::vector<market_group> get_market_group(int id);
    std::vector<market> get_market_quote(int id);
    market_details_response get_market_details(int id);
    trade_response trade(const trade_request &&request);
    std::vector<candle> backfill(int market_id, int quote_id, size_t sz,
                                 chart_duration dur);

  private:
    rest_api rest_client_;
    ws_client ws_client_;
};
} // namespace td365
