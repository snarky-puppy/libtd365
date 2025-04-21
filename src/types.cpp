/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#include "types.h"

#include <parsing.h>

#include <nlohmann/json.hpp>


std::ostream &operator<<(std::ostream &os, const grouping &g) {
  os << to_string(g);
  return os;
}

std::ostream &operator<<(std::ostream &os, const direction &d) {
  os << to_string(d);
  return os;
}

std::ostream &operator<<(std::ostream &os, const tick &t) {
  auto tp = t.timestamp;
  auto tp_conv =
      std::chrono::time_point_cast<std::chrono::system_clock::duration>(tp);
  std::time_t time = std::chrono::system_clock::to_time_t(tp_conv);

  // Get milliseconds component
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
              tp.time_since_epoch()) %
            1000;

  os << "Tick { "
      << "quote_id: " << t.quote_id << ", bid: " << t.bid << ", ask: " << t.ask
      << ", spread: " << (t.ask - t.bid) << ", change: " << t.daily_change
      << ", dir: " << t.dir << ", high: " << t.high
      << ", low: " << t.low << ", mid: " << t.mid_price
      << ", tradable: " << t.tradable
      << ", call_only: " << t.call_only
      << ", field13: " << t.field13

      << ", time: " << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
      << '.' << std::setw(3) << std::setfill('0') << ms.count()
      << ", latency: " << (t.latency.count() / 1000000.0) << "ms"
      << ", type: " << t.group << " }";
  return os;
}

#ifdef __cpp_lib_print
// Support for std::print and std::println in C++23
template<typename CharT>
struct std::formatter<tick, CharT> : std::formatter<std::string, CharT> {
  auto format(const tick &t, std::format_context &ctx) const {
    std::ostringstream oss;
    oss << t;
    return std::formatter<std::string, CharT>::format(oss.str(), ctx);
  }
};
#endif

// Serialization
void to_json(nlohmann::json &j, const market_group &mg) {
  j = {
    {"ID", mg.id},
    {"Name", mg.name},
    {"IsSuperGroup", mg.is_super_group},
    {"IsWhiteLabelPopularMarket", mg.is_white_label_popular_market},
    {"HasSubscription", mg.has_subscription}
  };
}

// Deserialization
void from_json(const nlohmann::json &j, market_group &mg) {
  j.at("ID").get_to(mg.id);
  j.at("Name").get_to(mg.name);
  j.at("IsSuperGroup").get_to(mg.is_super_group);
  j.at("IsWhiteLabelPopularMarket").get_to(mg.is_white_label_popular_market);
  j.at("HasSubscription").get_to(mg.has_subscription);
}

// Serialization
void to_json(nlohmann::json &j, const market &m) {
  j = {
    {"MarketID", m.market_id},
    {"QuoteID", m.quote_id},
    {"AtQuoteAtMarket", m.at_quote_at_market},
    {"ExchangeID", m.exchange_id},
    {"PrcGenFractionalPrice", m.prc_gen_fractional_price},
    {"PrcGenDecimalPlaces", m.prc_gen_decimal_places},
    {"High", m.high},
    {"Low", m.low},
    {"DailyChange", m.daily_change},
    {"Bid", m.bid},
    {"Ask", m.ask},
    {"BetPer", m.bet_per},
    {"IsGSLPercent", m.is_gsl_percent},
    {"GSLDis", m.gsl_dis},
    {"MinCloseOrderDisTicks", m.min_close_order_dis_ticks},
    {"MinOpenOrderDisTicks", m.min_open_order_dis_ticks},
    {"DisplayBetPer", m.display_bet_per},
    {"IsInPortfolio", m.is_in_portfolio},
    {"Tradable", m.tradable},
    {"TradeOnWeb", m.trade_on_web},
    {"CallOnly", m.call_only},
    {"MarketName", m.market_name},
    {"TradeStartTime", m.trade_start_time},
    {"Currency", m.currency},
    {"AllowGtdsStops", m.allow_gtds_stops},
    {"ForceOpen", m.force_open},
    {"Margin", m.margin},
    {"MarginType", m.margin_type},
    {"GSLCharge", m.gsl_charge},
    {"IsGSLChargePercent", m.is_gsl_charge_percent},
    {"Spread", m.spread},
    {"TradeRateType", m.trade_rate_type},
    {"OpenTradeRate", m.open_trade_rate},
    {"CloseTradeRate", m.close_trade_rate},
    {"MinOpenTradeRate", m.min_open_trade_rate},
    {"MinCloseTradeRate", m.min_close_trade_rate},
    {"PriceDecimal", m.price_decimal},
    {"Subscription", m.subscription},
    {"SuperGroupID", m.super_group_id}
  };
}

// Deserialization
void from_json(const nlohmann::json &j, market &m) {
  j.at("MarketID").get_to(m.market_id);
  j.at("QuoteID").get_to(m.quote_id);
  j.at("AtQuoteAtMarket").get_to(m.at_quote_at_market);
  j.at("ExchangeID").get_to(m.exchange_id);
  j.at("PrcGenFractionalPrice").get_to(m.prc_gen_fractional_price);
  j.at("PrcGenDecimalPlaces").get_to(m.prc_gen_decimal_places);
  j.at("High").get_to(m.high);
  j.at("Low").get_to(m.low);
  j.at("DailyChange").get_to(m.daily_change);
  j.at("Bid").get_to(m.bid);
  j.at("Ask").get_to(m.ask);
  j.at("BetPer").get_to(m.bet_per);
  j.at("IsGSLPercent").get_to(m.is_gsl_percent);
  j.at("GSLDis").get_to(m.gsl_dis);
  j.at("MinCloseOrderDisTicks").get_to(m.min_close_order_dis_ticks);
  j.at("MinOpenOrderDisTicks").get_to(m.min_open_order_dis_ticks);
  j.at("DisplayBetPer").get_to(m.display_bet_per);
  j.at("IsInPortfolio").get_to(m.is_in_portfolio);
  j.at("Tradable").get_to(m.tradable);
  j.at("TradeOnWeb").get_to(m.trade_on_web);
  j.at("CallOnly").get_to(m.call_only);
  j.at("MarketName").get_to(m.market_name);
  j.at("TradeStartTime").get_to(m.trade_start_time);
  j.at("Currency").get_to(m.currency);
  j.at("AllowGtdsStops").get_to(m.allow_gtds_stops);
  j.at("ForceOpen").get_to(m.force_open);
  j.at("Margin").get_to(m.margin);
  j.at("MarginType").get_to(m.margin_type);
  j.at("GSLCharge").get_to(m.gsl_charge);
  j.at("IsGSLChargePercent").get_to(m.is_gsl_charge_percent);
  j.at("Spread").get_to(m.spread);
  j.at("TradeRateType").get_to(m.trade_rate_type);
  j.at("OpenTradeRate").get_to(m.open_trade_rate);
  j.at("CloseTradeRate").get_to(m.close_trade_rate);
  j.at("MinOpenTradeRate").get_to(m.min_open_trade_rate);
  j.at("MinCloseTradeRate").get_to(m.min_close_trade_rate);
  j.at("PriceDecimal").get_to(m.price_decimal);
  j.at("Subscription").get_to(m.subscription);
  j.at("SuperGroupID").get_to(m.super_group_id);
}

void to_json(nlohmann::json &j, const tick &m) {
  j = nlohmann::json{
    {"quote_id", m.quote_id},
    {"bid", m.bid},
    {"ask", m.ask},
    {"daily_change", m.daily_change},
    {"dir", m.dir},
    {"tradable", m.tradable},
    {"high", m.high},
    {"low", m.low},
    {"hash", m.hash},
    {"call_only", m.call_only},
    {"mid_price", m.mid_price},
    {"timestamp", m.timestamp.time_since_epoch().count()},
    {"field13", m.field13},
    {"group", m.group},
    {"latency", m.latency.count()}
  };
}

void from_json(const nlohmann::json &j, tick &m) {
  j.at("quote_id").get_to(m.quote_id);
  j.at("bid").get_to(m.bid);
  j.at("ask").get_to(m.ask);
  j.at("daily_change").get_to(m.daily_change);
  j.at("dir").get_to(m.dir);
  j.at("tradable").get_to(m.tradable);
  j.at("high").get_to(m.high);
  j.at("low").get_to(m.low);
  j.at("hash").get_to(m.hash);
  j.at("call_only").get_to(m.call_only);
  j.at("mid_price").get_to(m.mid_price);
  auto ts_count = j.at("timestamp").get<long long>();
  m.timestamp = std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>(
    std::chrono::nanoseconds(ts_count));
  j.at("field13").get_to(m.field13);
  j.at("group").get_to(m.group);
  auto latency_count = j.at("latency").get<long long>();
  m.latency = std::chrono::nanoseconds(latency_count);
}
