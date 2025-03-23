/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#ifndef TD365_H
#define TD365_H

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct market_group {
  int id;
  std::string name;
  bool is_super_group;
  bool is_white_label_popular_market; // if true, not really a super group. call
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

// Forward declaration for stream operator
std::ostream &operator<<(std::ostream &os, const tick &t);

class td365 {
public:
  // Tick callback function type
  using tick_callback = std::function<void(const tick &)>;

  explicit td365(tick_callback callback = nullptr);

  ~td365();

  void connect(const std::string &username, const std::string &password,
               const std::string &account_id);

  void connect_demo() const;

  std::vector<market_group> get_market_super_group() const;

  std::vector<market_group> get_market_group(int id) const;

  std::vector<market> get_market_quote(int id) const;

  void subscribe(int quote_id) const;

  // Block until the WebSocket connection is closed
  void wait_for_disconnect() const;

private:
  std::unique_ptr<class platform> platform_;
};

#endif // TD365_H
