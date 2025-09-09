/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#pragma once

#include <boost/asio/detail/descriptor_ops.hpp>
#include <boost/beast/websocket/stream_base.hpp>
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

    enum class chart_duration { m1 };

    struct tick {
        using time_type = std::chrono::time_point<std::chrono::system_clock,
            std::chrono::nanoseconds>;
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
        time_type timestamp;
        int field13; // Unknown field
        grouping group;
        std::chrono::nanoseconds latency{}; // difference between received timestamp
        // and timestamp sent by server

        static tick create(const std::string_view line);

        void parse(const std::string_view line);
    };

    struct trade_request {
        enum class direction { buy, sell };

        direction dir;
        int market_id;
        int quote_id;
        double price;
        double stake;
        double stop;
        double limit;
        std::string key;
    };

    struct trade_response {
    };

    struct account_summary {
        std::string account_id;
        int platform_id;
        double account_valuation;
        std::string funded_percentage_string;
        int client_id;
        std::string trading_account_type;
        double margin;
        double open_pnl_quote;
        double account_balance;
        double credit;
        double waived_margin;
        double resources;
        double change_imr;
        double variation_margin_required;
    };

    // Alerts wrapper
    struct alert_list {
        int total_records;
        // std::vector<nlohmann::json> records;
    };

    void to_json(nlohmann::json &j, alert_list const &a);

    void from_json(nlohmann::json const &j, alert_list &a);

    // Currency record
    struct currency_record {
        double account_balance;
        double account_valuation;
        double credit_allocation;
        std::string currency;
        std::string currency_code;
        std::string currency_symbol;
        double initial_margin;
        bool is_total;
        std::string margin_percentage;
        double open_pl;
        std::string percentage;
        int status;
        double trading_resources;
        double variation_margin_required;
        double waived_initial_margin_limit;
        int pt;
    };

    void to_json(nlohmann::json &j, currency_record const &c);

    void from_json(nlohmann::json const &j, currency_record &c);

    // Currencies wrapper
    struct currency_set {
        std::vector<currency_record> records;
        int status;
        int total_records;
    };

    void to_json(nlohmann::json &j, currency_set const &s);

    void from_json(nlohmann::json const &j, currency_set &s);

    // OpeningOrders wrapper
    struct opening_orders {
        // TODO:
        // std::vector<nlohmann::json> records;
        int status;
        int total_records;
    };

    void to_json(nlohmann::json &j, opening_orders const &o);

    void from_json(nlohmann::json const &j, opening_orders &o);

    // Position record
    struct position_record {
        double bet_per;
        std::string creation_time;
        std::string creation_time_utc;
        std::string currency_code;
        std::string currency_symbol;
        std::string current_price;
        double current_price_decimal;
        std::string direction;
        std::string expiry_date_time;
        double imr;
        bool is_rolling_market;
        bool is_total;
        bool is_triggered;
        std::string limit_order_price;
        double limit_order_price_decimal;
        double margin_factor;
        int market_id;
        std::string market_name;
        double notional_value;
        double open_pl;
        std::string opening_price;
        double opening_price_decimal;
        int64_t order_id;
        std::string order_type;
        int64_t position_id;
        int prc_gen_decimal_places;
        int64_t quote_id;
        double stake;
        std::string stop_order_price;
        double stop_order_price_decimal;
        std::string stop_type;
        bool tradable;
        std::string type;
    };

    void to_json(nlohmann::json &j, position_record const &p);

    void from_json(nlohmann::json const &j, position_record &p);

    // Positions wrapper
    struct position_set {
        std::vector<position_record> records;
        int status;
        int total_records;
    };

    void to_json(nlohmann::json &j, position_set const &s);

    void from_json(nlohmann::json const &j, position_set &s);

    // Top-level AccountDetails
    struct account_details {
        alert_list alerts;
        std::int64_t calculated_utc_ticks;
        int client_id;
        int client_language_id;
        currency_set currencies;
        opening_orders opening_orders_data;
        position_set positions;
        std::string trading_account_type;
    };

    void to_json(nlohmann::json &j, account_details const &a);

    void from_json(nlohmann::json const &j, account_details &a);

    struct trade_details {
    };

    struct market_details {
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

    struct client_web_option_info {
        double cfd_default_stake;
        bool is_deal_alway_hedge;
        bool is_deal_alway_guarantee;
        bool is_one_click_trade;
        bool is_order_alway_hedge;
        bool is_order_alway_guarantee;
        int stop_type_id;
        int trade_order_type_id;
        double deal_default_stake;
        double order_default_stake;
        double web_min_stake;
        double web_max_stake;
    };

    struct market_details_response {
        market_details market_details_data;
        client_web_option_info web_info;
    };

    // only needed for market orders
    struct request_trade_simulate {
        int market_id;
        int quote_id;
        double price;
        double stake;
        int trade_type; // 0 = hedge, 1 = not hedged
        bool trade_mode; // false = sell, true = buy
        bool has_closing_order;
        bool is_guaranteed;
        int order_mode_id;
        int order_type_id;
        int order_price_mode_id;
        std::string limit_order_price; // API expects these as JSON strings
        std::string stop_order_price; // if stop/trailing,
        int trailing_point;
        int close_position_id;
        bool is_kaazing_feed;
        std::string user_agent;
        std::string key;
    };

    struct order_state {
        enum class direction { buy, sell, none };

        direction direction = direction::none; // Selected buy/sell
        double stake = 0.0; // User-entered stake
        bool trailing = false; // Is trailing stop enabled?
        bool guarantee = false; // Is guaranteed stop enabled?
        double order_level = 0.0; // Price or points for order
        int good_until = 2; // 1=GFD, 2=EOD
        bool has_stop = false; // Stop leg present?
        double stop_point = 0.0; // Stop distance in points
        bool stop_by_point = true; // Stop by points vs price
        bool has_limit = false; // Limit leg present?
        double limit_point = 0.0; // Limit distance in points
        bool limit_by_point = true; // Limit by points vs price
        bool order_has_ido = false; // If-Done order present?
    };

    struct candle {
        std::chrono::time_point<std::chrono::system_clock> timestamp;
        double open;
        double high;
        double low;
        double close;
        double volume;
    };

    struct user_callbacks {
        using tick_cb_type = std::function<void(tick &&)>;
        using acc_summary_type = std::function<void(account_summary &&)>;
        using acc_details_type = std::function<void(account_details &&)>;
        using trade_response_cb_type = std::function<void(trade_response &&)>;

        tick_cb_type tick_cb = [](tick &&) {
        };
        acc_summary_type acc_summary_cb = [](account_summary &&) {
        };
        acc_details_type acc_detail_cb = [](account_details &&) {
        };
        trade_response_cb_type trade_response_cb = [](trade_response &&) {
        };
    };

    std::ostream &operator<<(std::ostream &os, const td365::market_group &);

    std::ostream &operator<<(std::ostream &os, const td365::market &);

    std::ostream &operator<<(std::ostream &os, const td365::grouping &);

    std::ostream &operator<<(std::ostream &os, const td365::direction &);

    std::ostream &operator<<(std::ostream &os, const td365::tick &);

    std::ostream &operator<<(std::ostream &os, const td365::account_summary &);

    std::ostream &operator<<(std::ostream &os, const td365::account_details &);

    void to_json(nlohmann::json &j, const td365::market_group &mg);

    void from_json(const nlohmann::json &j, td365::market_group &mg);

    void to_json(nlohmann::json &j, const td365::market &m);

    void from_json(const nlohmann::json &j, td365::market &m);

    void to_json(nlohmann::json &j, const td365::tick &m);

    void from_json(const nlohmann::json &j, td365::tick &m);

    void to_json(nlohmann::json &j, const td365::market_details &m);

    void from_json(const nlohmann::json &j, td365::market_details &m);

    void to_json(nlohmann::json &j, const td365::client_web_option_info &c);

    void from_json(const nlohmann::json &j, td365::client_web_option_info &c);

    void to_json(nlohmann::json &j, account_summary const &a);

    void from_json(nlohmann::json const &j, account_summary &a);

    void to_json(nlohmann::json &j, td365::candle const &c);

    void from_json(nlohmann::json const &j, td365::candle &c);

    void to_json(nlohmann::json &j, const td365::trade_response &);

    void from_json(const nlohmann::json &j, td365::trade_response &);

    void from_json(const nlohmann::json &j, td365::market_details_response &r);
} // namespace td365
