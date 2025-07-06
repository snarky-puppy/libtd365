/*
 * The Prosperity Public License 3.0.0
 *
 * Copyright (c) 2025, Matt Wlazlo
 *
 * Contributor: Matt Wlazlo
 * Source Code: https://github.com/snarky-puppy/td365
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#include "td365.h"

#include <algorithm>
#include <boost/core/verbose_terminate_handler.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <cassert>
#include <iostream>
#include <spdlog/spdlog.h>
#include <vector>

struct candle_agg {
    static constexpr int CandleSeconds = 60;
    std::array<td365::candle, 3>
        history{}; // [0]=current, [1]=prev, [2]=two ago
    td365::candle current{};
    size_t elapsed_candles = 0;
    int64_t current_bucket = tick_bucket(std::chrono::system_clock::now());

    int64_t tick_bucket(const td365::tick::time_type &t) {
        auto secs = std::chrono::duration_cast<std::chrono::seconds>(
                        t.time_since_epoch())
                        .count();
        return secs / CandleSeconds;
    }

    int n_ticks = 0;

    void on_tick(const td365::tick &t) {
        auto bucket = tick_bucket(t.timestamp);

        double price = t.mid_price;

        if (bucket != current_bucket) {
            // 2. New minute → roll history down
            auto dt = std::format("{}", current.timestamp);

            spdlog::info("new candle: dt={} o={} h={} l={} close={}", dt,
                         current.open, current.high, current.low,
                         current.close);
            for (size_t i = history.size() - 1; i > 0; --i) {
                history[i] = history[i - 1];
            }
            history[0] = current;

            // 3. Start fresh candle
            current = td365::candle{
                .timestamp = std::chrono::system_clock::now(),
                .open = price,
                .high = price,
                .low = price,
                .close = price,
            };

            current_bucket = bucket;
            elapsed_candles++;
        } else {
            current.high = std::max(current.high, price);
            current.low = std::min(current.low, price);
            current.close = price;
        }
    }

    bool trending_up() const {
        return elapsed_candles > 2 && history[0].close > history[1].close &&
               history[1].open > history[2].open;
    }

    bool trending_down() const {
        return elapsed_candles > 2 && history[0].close < history[1].close &&
               history[1].open < history[2].open;
    }
    void backfill(const std::vector<td365::candle> &v) {
        size_t i = 0;
        for (auto &c : v) {
            history[i++] = c;
        }
    }
};

struct signals {
    enum class value { none, buy, sell };
    value last_signal = value::none;
    int n_ticks = 0;

    auto alert(value v) {
        if (v == last_signal) {
            return value::none;
        }
        last_signal = v;
        return v;
    }

    auto on_tick(const td365::tick &t) -> value {
        agg.on_tick(t);
        if (agg.trending_up()) {
            return alert(value::buy);
        }
        if (agg.trending_down()) {
            return alert(value::sell);
        }
        return value::none;
    }

    candle_agg agg;
};

struct strategy {

    explicit strategy(boost::asio::any_io_executor ioc) {
        client.callbacks() = td365::user_callbacks{
            .tick_cb =
                [&](td365::tick &&t) {
                    boost::asio::post(ioc, [this, t = std::move(t)]() mutable {
                        on_tick(std::move(t));
                    });
                },
            .acc_detail_cb =
                [&](td365::account_details &&a) {
                    boost::asio::post(ioc, [this, a = std::move(a)]() mutable {
                        on_account_details(std::move(a));
                    });
                },
            .acc_summary_cb =
                [&](td365::account_summary &&a) {
                    boost::asio::post(ioc, [this, a = std::move(a)]() mutable {
                        on_account_summary(std::move(a));
                    });
                },
            .trade_response_cb =
                [&](td365::trade_response &&r) {
                    boost::asio::post(ioc, [this, r = std::move(r)]() mutable {
                        on_trade_response(std::move(r));
                    });
                },
        };

        client.connect();
    }

    int n_ticks = 0;

    void on_tick(td365::tick &&t) {
        if (n_ticks == 10) {
            buy(t);
        }
        n_ticks++;
        spdlog::info("tick: {}", n_ticks);
        switch (signals.on_tick(t)) {
        case signals::value::none:
            break;
        case signals::value::buy:
            spdlog::info("signals::value::buy");
            buy(t);
            break;
        case signals::value::sell:
            spdlog::info("signals::value::sell");
            sell(t);
            break;
        }
    }

    void buy(const td365::tick &t) {
        client.trade({
            .dir = td365::trade_request::direction::buy,
            .quote_id = market.quote_id,
            .market_id = market.market_id,
            .price = t.ask,
            .stake = 1,
            .limit = t.ask + 10,
            .stop = t.ask - 10,
            .key = t.hash,
        });
    }

    void sell(const td365::tick &t) {
        client.trade({
            .dir = td365::trade_request::direction::sell,
            .quote_id = market.quote_id,
            .market_id = market.market_id,
            .price = t.bid,
            .stake = 1,
            .limit = t.bid - 10,
            .stop = t.bid + 10,
            .key = t.hash,
        });
    }

    void on_account_summary(td365::account_summary &&) {
        spdlog::info("on_account_summary");
    }
    void on_account_details(td365::account_details &&) {
        spdlog::info("on_account_details");
    }

    void on_trade_response(td365::trade_response &&) {
        spdlog::info("on_trade_response");
    }

    void setup_subscription() {
        auto super_groups = client.get_market_super_group();
        auto indices = std::ranges::find_if(super_groups, [](const auto &x) {
            return x.name.compare("Indices") == 0;
        });
        assert(indices != super_groups.end());

        auto second_level = client.get_market_group(indices->id);
        auto us_item = std::ranges::find_if(second_level, [](const auto &x) {
            return x.name.compare("US") == 0;
        });
        assert(us_item != second_level.end());

        auto us = client.get_market_quote(us_item->id);
        auto nasdaq = std::ranges::find_if(us, [](const auto &x) {
            return x.market_name.compare("US Tech 100") == 0;
        });
        assert(nasdaq != us.end());

        market = *nasdaq;
        client.subscribe(market.quote_id);
    }

    void backfill() {
        auto candles = client.backfill(market.market_id, market.quote_id, 3,
                                       td365::chart_duration::m1);
        signals.agg.backfill(candles);
    }

    td365::td365 client;
    signals signals;
    td365::market market;
};

int main(int, char **) {
    std::set_terminate(boost::core::verbose_terminate_handler);

    boost::asio::io_context ioc;
    spdlog::set_level(spdlog::level::debug);

    auto strat = strategy(ioc.get_executor());
    strat.setup_subscription();
    strat.backfill();

    boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
        work_guard(ioc.get_executor());
    ioc.run();

    return 0;
}
