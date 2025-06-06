/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#pragma once

#include <chrono>
#include <functional>
#include <nlohmann/json_fwd.hpp>
#include <string>

namespace td365 {
struct market_group {
    int id;
    std::string name;
    bool is_super_group;
    bool is_white_label_popular_market; // if true, not really a super group.
    // call
    // get_market_quote directly.
    bool has_subscription;
};

struct market {
    int market_id;
    int quote_id;
    int at_quote_at_market;
    int exchange_id;
    int prc_gen_fractional_price;
    int prc_gen_decimal_places;
    double high;
    double low;
    double daily_change;
    double bid;
    double ask;
    double bet_per;
    int is_gsl_percent;
    double gsl_dis;
    double min_close_order_dis_ticks;
    double min_open_order_dis_ticks;
    double display_bet_per;
    bool is_in_portfolio;
    bool tradable;
    bool trade_on_web;
    bool call_only;
    std::string market_name;
    std::string trade_start_time;
    std::string currency;
    int allow_gtds_stops;
    bool force_open;
    double margin;
    bool margin_type;
    double gsl_charge;
    int is_gsl_charge_percent;
    double spread;
    int trade_rate_type;
    double open_trade_rate;
    double close_trade_rate;
    double min_open_trade_rate;
    double min_close_trade_rate;
    double price_decimal;
    bool subscription;
    int super_group_id;
};

// Enum for price data types
enum class grouping { grouped, sampled, delayed, candle_1m, _count };

enum class direction { up, down, unchanged, _count };

struct tick {
    int quote_id;
    double bid;
    double ask;
    double daily_change;
    direction dir;
    bool tradable;
    double high;
    double low;
    std::string hash; // Base64 encoded hash
    bool call_only;
    double mid_price;
    std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>
        timestamp;
    int field13; // Unknown field
    grouping group;
    std::chrono::nanoseconds latency{}; // difference between received timestamp
    // and timestamp sent by server
};

struct account_summary {};

struct account_details {};

std::ostream &operator<<(std::ostream &os, const market_group &);

std::ostream &operator<<(std::ostream &os, const market &);

std::ostream &operator<<(std::ostream &os, const grouping &);

std::ostream &operator<<(std::ostream &os, const direction &);

std::ostream &operator<<(std::ostream &os, const tick &);

std::ostream &operator<<(std::ostream &os, const account_summary &);

std::ostream &operator<<(std::ostream &os, const account_details &);

void to_json(nlohmann::json &j, const market_group &mg);

void from_json(const nlohmann::json &j, market_group &mg);

void to_json(nlohmann::json &j, const market &m);

void from_json(const nlohmann::json &j, market &m);

void to_json(nlohmann::json &j, const tick &m);

void from_json(const nlohmann::json &j, tick &m);

struct user_callbacks {
    using tick_callback = std::function<void(tick &&)>;
    using account_summary_callback = std::function<void(account_summary &&)>;
    using account_details_callback = std::function<void(account_details &&)>;

    tick_callback tick_cb = nullptr;
    account_summary_callback account_sum_cb = nullptr;
    account_details_callback account_detail_cb = nullptr;

    void on_tick(tick &&t) const {
        if (tick_cb) {
            tick_cb(std::move(t));
        }
    }

    void on_account_summary(account_summary &&a) const {
        if (account_sum_cb) {
            account_sum_cb(std::move(a));
        }
    }

    void on_account_details(account_details &&a) const {
        if (account_detail_cb) {
            account_detail_cb(std::move(a));
        }
    }
};
} // namespace td365
