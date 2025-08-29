/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#pragma once

#include <td365/td365.h>

#include <optional>
#include <string_view>
#include <unordered_map>

namespace td365 {
static const std::unordered_map<std::string_view, grouping> grouping_map = {
    {"gp", grouping::grouped},
    {"sp", grouping::sampled},
    {"dp", grouping::delayed},
    {"c1m", grouping::candle_1m}};

// Convert price_type enum to string
std::string_view to_string(grouping pt);

const std::string &to_cstring(grouping pt);

// Convert direction enum to string
std::string_view to_string(direction dir);

const std::string &to_cstring(direction dir);

// Convert string to price_type
grouping string_to_price_type(std::string_view key);

// Parse tick from a comma-separated string
// Format:
// "quote_id,bid,ask,daily_change,direction,field6,high,low,hash,field10,mid_price,timestamp,field13"
tick parse_tick(const std::string &price_string, grouping price_type);
tick parse_tick2(std::string_view price_string, grouping price_type);

candle parse_candle(std::string_view candle_string);

} // namespace td365
