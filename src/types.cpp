/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#include <boost/charconv.hpp>
#include <nlohmann/json.hpp>
#include <ranges>
#include <sstream>
#include <td365/parsing.h>
#include <td365/types.h>

namespace td365 {
std::ostream &operator<<(std::ostream &os, const grouping &g) {
    os << to_string(g);
    return os;
}

std::ostream &operator<<(std::ostream &os, const direction &d) {
    os << to_string(d);
    return os;
}

std::ostream &operator<<(std::ostream &os, const tick &t) {
    /*
    auto tp = t.timestamp;
    auto tp_conv =
        std::chrono::time_point_cast<std::chrono::system_clock::duration>(tp);
    std::time_t time = std::chrono::system_clock::to_time_t(tp_conv);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  tp.time_since_epoch()) %
              1000;
    std::ostringstream iso_timestamp;
    iso_timestamp << std::put_time(std::localtime(&time), "%Y-%m-%dT%H:%M:%S")
                  << '.' << std::setw(3) << std::setfill('0') << ms.count()
                  << "Z";
                  */

    auto latency = t.latency.count();

    os << t.quote_id << "," << std::fixed << std::setprecision(6) << t.bid
       << "," << std::fixed << std::setprecision(6) << t.ask << ","
       << std::fixed << std::setprecision(6) << t.daily_change << ","
       << "" << to_string(t.dir) << "," << (t.tradable ? "true" : "false")
       << "," << std::fixed << std::setprecision(6) << t.high << ","
       << std::fixed << std::setprecision(6) << t.low << "," << t.hash << ","
       << (t.call_only ? "true" : "false") << "," << std::fixed
       << std::setprecision(6) << t.mid_price << ","
       << "" << t.timestamp << "," << t.field13 << ","
       << "" << to_cstring(t.group) << "," << latency;
    return os;
}

void tick::parse(const std::string_view line) {
    constexpr size_t EXPECTED_FIELDS = 15;
    std::array<std::string_view, EXPECTED_FIELDS> fields;
    size_t idx = 0;

    // Split on ',' into subranges
    for (auto sub :
         line | std::views::split(',') | std::views::transform([](auto rng) {
             auto first = rng.begin();
             auto len = std::ranges::size(rng);
             return std::string_view(&*first, len);
         })) {
        if (idx < EXPECTED_FIELDS) {
            fields[idx++] = sub;
        } else {
            break;
        }
    }

    if (idx != EXPECTED_FIELDS) {
        throw std::invalid_argument("Invalid CSV tick format: expected " +
                                    std::to_string(EXPECTED_FIELDS) +
                                    " fields, got " + std::to_string(idx));
    }

    // Parse direction from string
    if (fields[4][1] == 'p') {
        // 'up'
        dir = direction::up;
    } else if (fields[4][0] == 'd') {
        dir = direction::down;
    } else {
        dir = direction::unchanged;
    }

    // Parse timestamp (ISO 8601 format: YYYY-MM-DDTHH:MM:SS.sssZ)
    /*
            if (timestamp_str.size() >= 23 && timestamp_str.back() == 'Z') {
                // Parse YYYY-MM-DDTHH:MM:SS.sssZ
                std::tm tm{};
                auto date_part = timestamp_str.substr(0, 19);
                auto ms_part = timestamp_str.substr(20, 3);

                auto date_part_s = std::string(date_part);
                std::istringstream ss(date_part_s);
                ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");

                if (ss.fail()) {
                    throw std::invalid_argument("Invalid timestamp format: " +
                                                std::string(timestamp_str));
                }

                std::time_t time = std::mktime(&tm);
                auto ms =
       std::chrono::milliseconds(std::stoi(std::string(ms_part))); timestamp =
       std::chrono::system_clock::from_time_t(time) + ms; } else {
                spdlog::error("Invalid timestamp format: {}", timestamp_str);
                spdlog::error("on line: {}", line);
                throw std::invalid_argument("Invalid timestamp format: " +
                                            std::string(timestamp_str));
            }
    */

    // Parse grouping enum
    group = string_to_price_type(fields[13]);

    // Helper lambda to parse double
    auto parse_double = [](std::string_view sv) -> double {
        double value{};
        auto result = boost::charconv::from_chars(sv.data(),
                                                  sv.data() + sv.size(), value);
        if (result.ec != std::errc{}) {
            throw std::invalid_argument("Invalid double: " + std::string(sv));
        }
        return value;
    };

    // Helper lambda to parse int
    auto parse_int = [](std::string_view sv) -> int {
        int value{};
        auto result = boost::charconv::from_chars(sv.data(),
                                                  sv.data() + sv.size(), value);
        if (result.ec != std::errc{}) {
            throw std::invalid_argument("Invalid int: " + std::string(sv));
        }
        return value;
    };

    // Helper lambda to parse bool
    auto parse_bool = [](std::string_view sv) -> bool { return sv == "true"; };

    quote_id = parse_int(fields[0]);
    bid = parse_double(fields[1]);
    ask = parse_double(fields[2]);
    daily_change = parse_double(fields[3]);
    tradable = parse_bool(fields[5]);
    high = parse_double(fields[6]);
    low = parse_double(fields[7]);
    hash = std::string(fields[8]);
    call_only = parse_bool(fields[9]);
    mid_price = parse_double(fields[10]);
    field13 = parse_int(fields[12]);
    latency = string_to_duration(fields[14]);
    timestamp = string_to_timepoint(fields[11]);
}

tick tick::create(const std::string_view line) {
    tick rv;
    rv.parse(line);
    return rv;
}

/*
#ifdef __cpp_lib_print
// Support for std::print and std::println in C++23
template <typename CharT>
struct std::formatter<tick, CharT> : std::formatter<std::string, CharT> {
    auto format(const tick &t, std::format_context &ctx) const {
        std::ostringstream oss;
        oss << t;
        return std::formatter<std::string, CharT>::format(oss.str(), ctx);
    }
};
#endif
*/

// Serialization
void to_json(nlohmann::json &j, const market_group &mg) {
    j = {{"ID", mg.id},
         {"Name", mg.name},
         {"IsSuperGroup", mg.is_super_group},
         {"IsWhiteLabelPopularMarket", mg.is_white_label_popular_market},
         {"HasSubscription", mg.has_subscription}};
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
    j = {{"MarketID", m.market_id},
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
         {"SuperGroupID", m.super_group_id}};
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
    j = nlohmann::json{{"quote_id", m.quote_id},
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
                       {"latency", m.latency.count()}};
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
    m.timestamp = std::chrono::time_point<std::chrono::system_clock,
                                          std::chrono::nanoseconds>(
        std::chrono::nanoseconds(ts_count));
    j.at("field13").get_to(m.field13);
    j.at("group").get_to(m.group);
    auto latency_count = j.at("latency").get<long long>();
    m.latency = std::chrono::nanoseconds(latency_count);
}

void to_json(nlohmann::json &j, request_trade_simulate const &r) {
    j = nlohmann::json{{"marketID", r.market_id},
                       {"quoteID", r.quote_id},
                       {"price", r.price},
                       {"stake", r.stake},
                       {"tradeType", r.trade_type},
                       {"tradeMode", r.trade_mode},
                       {"hasClosingOrder", r.has_closing_order},
                       {"isGuaranteed", r.is_guaranteed},
                       {"orderModeID", r.order_mode_id},
                       {"orderTypeID", r.order_type_id},
                       {"orderPriceModeID", r.order_price_mode_id},
                       {"limitOrderPrice", r.limit_order_price},
                       {"stopOrderPrice", r.stop_order_price},
                       {"trailingPoint", r.trailing_point},
                       {"closePositionID", r.close_position_id},
                       {"isKaazingFeed", r.is_kaazing_feed},
                       {"userAgent", r.user_agent},
                       {"key", r.key}};
}

void from_json(nlohmann::json const &j, request_trade_simulate &r) {
    j.at("marketID").get_to(r.market_id);
    j.at("quoteID").get_to(r.quote_id);
    j.at("price").get_to(r.price);
    j.at("stake").get_to(r.stake);
    j.at("tradeType").get_to(r.trade_type);
    j.at("tradeMode").get_to(r.trade_mode);
    j.at("hasClosingOrder").get_to(r.has_closing_order);
    j.at("isGuaranteed").get_to(r.is_guaranteed);
    j.at("orderModeID").get_to(r.order_mode_id);
    j.at("orderTypeID").get_to(r.order_type_id);
    j.at("orderPriceModeID").get_to(r.order_price_mode_id);
    j.at("limitOrderPrice").get_to(r.limit_order_price);
    j.at("stopOrderPrice").get_to(r.stop_order_price);
    j.at("trailingPoint").get_to(r.trailing_point);
    j.at("closePositionID").get_to(r.close_position_id);
    j.at("isKaazingFeed").get_to(r.is_kaazing_feed);
    j.at("userAgent").get_to(r.user_agent);
    j.at("key").get_to(r.key);
}

void from_json(const nlohmann::json &j, market_details &m) {
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

void from_json(const nlohmann::json &j, client_web_option_info &w) {
    j.at("CFDDefaultStake").get_to(w.cfd_default_stake);
    j.at("IsDealAlwayHedge").get_to(w.is_deal_alway_hedge);
    j.at("IsDealAlwayGuarantee").get_to(w.is_deal_alway_guarantee);
    j.at("IsOneClickTrade").get_to(w.is_one_click_trade);
    j.at("IsOrderAlwayHedge").get_to(w.is_order_alway_hedge);
    j.at("IsOrderAlwayGuarantee").get_to(w.is_order_alway_guarantee);
    j.at("StopTypeID").get_to(w.stop_type_id);
    j.at("TradeOrderTypeID").get_to(w.trade_order_type_id);
    j.at("DealDefaultStake").get_to(w.deal_default_stake);
    j.at("OrderDefaultStake").get_to(w.order_default_stake);
    j.at("WebMinStake").get_to(w.web_min_stake);
    j.at("WebMaxStake").get_to(w.web_max_stake);
}

// Serialize `candle` → JSON
void to_json(nlohmann::json &j, candle const &c) {
    j = nlohmann::json{{"open", c.open},
                       {"high", c.high},
                       {"low", c.low},
                       {"close", c.close},
                       {"volume", c.volume}};
}

// Deserialize JSON → `candle`
void from_json(nlohmann::json const &j, candle &c) {
    // .at() will throw if the key is missing
    j.at("open").get_to(c.open);
    j.at("high").get_to(c.high);
    j.at("low").get_to(c.low);
    j.at("close").get_to(c.close);
    j.at("volume").get_to(c.volume);
}

void to_json(nlohmann::json &, const trade_response &) {}

void from_json(const nlohmann::json &, trade_response &) {}

void from_json(const nlohmann::json &j, market_details_response &r) {
    j.at("marketDetails").get_to(r.market_details_data);
    j.at("webInfo").get_to(r.web_info);
}

void to_json(nlohmann::json &j, account_summary const &a) {
    j = nlohmann::json{
        {"AccountID", a.account_id},
        {"PlatformID", a.platform_id},
        {"AccountValuation", a.account_valuation},
        {"FundedPercentageString", a.funded_percentage_string},
        {"ClientId", a.client_id},
        {"TradingAccountType", a.trading_account_type},
        {"Margin", a.margin},
        {"OpenPnLQuote", a.open_pnl_quote},
        {"AccountBalance", a.account_balance},
        {"Credit", a.credit},
        {"WaivedMargin", a.waived_margin},
        {"Resources", a.resources},
        {"ChangeIMR", a.change_imr},
        {"VariationMarginRequired", a.variation_margin_required}};
}

void from_json(nlohmann::json const &j, account_summary &a) {
    j.at("AccountID").get_to(a.account_id);
    j.at("PlatformID").get_to(a.platform_id);
    j.at("AccountValuation").get_to(a.account_valuation);
    j.at("FundedPercentageString").get_to(a.funded_percentage_string);
    j.at("ClientId").get_to(a.client_id);
    j.at("TradingAccountType").get_to(a.trading_account_type);
    j.at("Margin").get_to(a.margin);
    j.at("OpenPnLQuote").get_to(a.open_pnl_quote);
    j.at("AccountBalance").get_to(a.account_balance);
    j.at("Credit").get_to(a.credit);
    j.at("WaivedMargin").get_to(a.waived_margin);
    j.at("Resources").get_to(a.resources);
    j.at("ChangeIMR").get_to(a.change_imr);
    j.at("VariationMarginRequired").get_to(a.variation_margin_required);
}

// Alerts
void to_json(nlohmann::json &j, alert_list const &a) {
    j = nlohmann::json{
        {"TotalRecords", a.total_records},
        /*{"records", a.records}*/
    };
}

void from_json(nlohmann::json const &j, alert_list &a) {
    j.at("TotalRecords").get_to(a.total_records);
    // j.at("records").get_to(a.records);
}

// CurrencyRecord
void to_json(nlohmann::json &j, currency_record const &c) {
    j = nlohmann::json{
        {"AccountBalance", c.account_balance},
        {"AccountValuation", c.account_valuation},
        {"CreditAllocation", c.credit_allocation},
        {"Currency", c.currency},
        {"CurrencyCode", c.currency_code},
        {"CurrencySymbol", c.currency_symbol},
        {"InitialMargin", c.initial_margin},
        {"IsTotal", c.is_total},
        {"MarginPercentage", c.margin_percentage},
        {"OpenPL", c.open_pl},
        {"Percentage", c.percentage},
        {"Status", c.status},
        {"TradingResources", c.trading_resources},
        {"VariationMarginRequired", c.variation_margin_required},
        {"WaivedInitialMarginLimit", c.waived_initial_margin_limit},
        {"pt", c.pt}};
}

void from_json(nlohmann::json const &j, currency_record &c) {
    j.at("AccountBalance").get_to(c.account_balance);
    j.at("AccountValuation").get_to(c.account_valuation);
    j.at("CreditAllocation").get_to(c.credit_allocation);
    j.at("Currency").get_to(c.currency);
    j.at("CurrencyCode").get_to(c.currency_code);
    j.at("CurrencySymbol").get_to(c.currency_symbol);
    j.at("InitialMargin").get_to(c.initial_margin);
    j.at("IsTotal").get_to(c.is_total);
    j.at("MarginPercentage").get_to(c.margin_percentage);
    j.at("OpenPL").get_to(c.open_pl);
    j.at("Percentage").get_to(c.percentage);
    j.at("Status").get_to(c.status);
    j.at("TradingResources").get_to(c.trading_resources);
    j.at("VariationMarginRequired").get_to(c.variation_margin_required);
    j.at("WaivedInitialMarginLimit").get_to(c.waived_initial_margin_limit);
    j.at("pt").get_to(c.pt);
}

// CurrencySet
void to_json(nlohmann::json &j, currency_set const &s) {
    j = nlohmann::json{{"Records", s.records},
                       {"Status", s.status},
                       {"TotalRecords", s.total_records}};
}

void from_json(nlohmann::json const &j, currency_set &s) {
    j.at("Records").get_to(s.records);
    j.at("Status").get_to(s.status);
    j.at("TotalRecords").get_to(s.total_records);
}

// OpeningOrders
void to_json(nlohmann::json &j, opening_orders const &o) {
    j = nlohmann::json{/*{"Records", o.records},*/
                       {"Status", o.status},
                       {"TotalRecords", o.total_records}};
}

void from_json(nlohmann::json const &j, opening_orders &o) {
    // j.at("Records").get_to(o.records);
    j.at("Status").get_to(o.status);
    j.at("TotalRecords").get_to(o.total_records);
}

// PositionRecord
void to_json(nlohmann::json &j, position_record const &p) {
    j = nlohmann::json{{"BetPer", p.bet_per},
                       {"CreationTime", p.creation_time},
                       {"CreationTimeUTC", p.creation_time_utc},
                       {"CurrencyCode", p.currency_code},
                       {"CurrencySymbol", p.currency_symbol},
                       {"CurrentPrice", p.current_price},
                       {"CurrentPriceDecimal", p.current_price_decimal},
                       {"Direction", p.direction},
                       {"ExpiryDateTime", p.expiry_date_time},
                       {"IMR", p.imr},
                       {"IsRollingMarket", p.is_rolling_market},
                       {"IsTotal", p.is_total},
                       {"IsTriggered", p.is_triggered},
                       {"LimitOrderPrice", p.limit_order_price},
                       {"LimitOrderPriceDecimal", p.limit_order_price_decimal},
                       {"MarginFactor", p.margin_factor},
                       {"MarketID", p.market_id},
                       {"MarketName", p.market_name},
                       {"NotionalValue", p.notional_value},
                       {"OpenPL", p.open_pl},
                       {"OpeningPrice", p.opening_price},
                       {"OpeningPriceDecimal", p.opening_price_decimal},
                       {"OrderID", p.order_id},
                       {"OrderType", p.order_type},
                       {"PositionID", p.position_id},
                       {"PrcGenDecimalPlaces", p.prc_gen_decimal_places},
                       {"QuoteID", p.quote_id},
                       {"Stake", p.stake},
                       {"StopOrderPrice", p.stop_order_price},
                       {"StopOrderPriceDecimal", p.stop_order_price_decimal},
                       {"StopType", p.stop_type},
                       {"Tradable", p.tradable},
                       {"Type", p.type}};
}

void from_json(nlohmann::json const &j, position_record &p) {
    j.at("BetPer").get_to(p.bet_per);
    j.at("CreationTime").get_to(p.creation_time);
    j.at("CreationTimeUTC").get_to(p.creation_time_utc);
    j.at("CurrencyCode").get_to(p.currency_code);
    j.at("CurrencySymbol").get_to(p.currency_symbol);
    j.at("CurrentPrice").get_to(p.current_price);
    j.at("CurrentPriceDecimal").get_to(p.current_price_decimal);
    j.at("Direction").get_to(p.direction);
    j.at("ExpiryDateTime").get_to(p.expiry_date_time);
    j.at("IMR").get_to(p.imr);
    j.at("IsRollingMarket").get_to(p.is_rolling_market);
    j.at("IsTotal").get_to(p.is_total);
    j.at("IsTriggered").get_to(p.is_triggered);
    j.at("LimitOrderPrice").get_to(p.limit_order_price);
    j.at("LimitOrderPriceDecimal").get_to(p.limit_order_price_decimal);
    j.at("MarginFactor").get_to(p.margin_factor);
    j.at("MarketID").get_to(p.market_id);
    j.at("MarketName").get_to(p.market_name);
    j.at("NotionalValue").get_to(p.notional_value);
    j.at("OpenPL").get_to(p.open_pl);
    j.at("OpeningPrice").get_to(p.opening_price);
    j.at("OpeningPriceDecimal").get_to(p.opening_price_decimal);
    j.at("OrderID").get_to(p.order_id);
    j.at("OrderType").get_to(p.order_type);
    j.at("PositionID").get_to(p.position_id);
    j.at("PrcGenDecimalPlaces").get_to(p.prc_gen_decimal_places);
    j.at("QuoteID").get_to(p.quote_id);
    j.at("Stake").get_to(p.stake);
    j.at("StopOrderPrice").get_to(p.stop_order_price);
    j.at("StopOrderPriceDecimal").get_to(p.stop_order_price_decimal);
    j.at("StopType").get_to(p.stop_type);
    j.at("Tradable").get_to(p.tradable);
    j.at("Type").get_to(p.type);
}

// PositionSet
void to_json(nlohmann::json &j, position_set const &s) {
    j = nlohmann::json{{"Records", s.records},
                       {"Status", s.status},
                       {"TotalRecords", s.total_records}};
}

void from_json(nlohmann::json const &j, position_set &s) {
    j.at("Records").get_to(s.records);
    j.at("Status").get_to(s.status);
    j.at("TotalRecords").get_to(s.total_records);
}

// AccountDetails
void to_json(nlohmann::json &j, account_details const &a) {
    j = nlohmann::json{{"Alerts", a.alerts},
                       {"CalculatedUTCTicks", a.calculated_utc_ticks},
                       {"ClientId", a.client_id},
                       {"ClientLanguageId", a.client_language_id},
                       {"Currencies", a.currencies},
                       {"OpeningOrders", a.opening_orders_data},
                       {"Positions", a.positions},
                       {"TradingAccountType", a.trading_account_type}};
}

void from_json(nlohmann::json const &j, account_details &a) {
    if (j.contains("Alerts"))
        j.at("Alerts").get_to(a.alerts);
    else
        a.alerts.total_records = 0;
    j.at("CalculatedUTCTicks").get_to(a.calculated_utc_ticks);
    j.at("ClientId").get_to(a.client_id);
    j.at("ClientLanguageId").get_to(a.client_language_id);
    if (j.contains("Currencies"))
        j.at("Currencies").get_to(a.currencies);
    else
        a.currencies.total_records = 0;
    if (j.contains("OpeningOrders"))
        j.at("OpeningOrders").get_to(a.opening_orders_data);
    else
        a.opening_orders_data.total_records = 0;
    if (j.contains("Positions"))
        j.at("Positions").get_to(a.positions);
    else
        a.positions.total_records = 0;
    j.at("TradingAccountType").get_to(a.trading_account_type);
}
} // namespace td365
