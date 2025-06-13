/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#include "parsing.h"

#include "types.h"

#include <cassert>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace td365 {
static const std::array<std::string, static_cast<size_t>(grouping::_count)>
    grouping_lookup = {"Grouped", "Sampled", "Delayed", "Candle1Minute"};

constexpr std::array<grouping, 256> faster_grouping_lookup = [] {
    std::array<grouping, 256> arr = {};
    arr['G'] = grouping::grouped;
    arr['S'] = grouping::sampled;
    arr['D'] = grouping::delayed;
    arr['C'] = grouping::candle_1m;
    return arr;
}();

static const std::array<std::string, static_cast<size_t>(direction::_count)>
    direction_lookup = {
        "up",
        "down",
        "unchanged",
};

// Convert direction enum to string for output
std::string_view to_string(direction dir) { return to_cstring(dir); }

const std::string &to_cstring(direction dir) {
    return direction_lookup[static_cast<size_t>(dir)];
}

// Convert price_type enum to string for output
std::string_view to_string(grouping pt) { return to_cstring(pt); }

const std::string &to_cstring(grouping pt) {
    return grouping_lookup[static_cast<size_t>(pt)];
}

// String to price_type conversion helper
grouping string_to_price_type(std::string_view key) {
    // auto it = grouping_map.find(key);
    // return (it != grouping_map.end()) ? it->second : grouping::unknown;
    return faster_grouping_lookup[static_cast<size_t>(key[0])];
}

// Parse a comma-separated price string into a tick object
tick parse_tick(const std::string &price_string, grouping price_type) {
    std::vector<std::string> fields;
    std::stringstream ss(price_string);
    std::string field;

    // Split by commas
    while (std::getline(ss, field, ',')) {
        fields.push_back(field);
    }

    // Ensure we have enough fields
    if (fields.size() < 13) {
        throw fail("Invalid price data format: {}", price_string);
    }

    // Parse direction
    direction dir_value;
    if (fields[4][0] == 'u') {
        dir_value = direction::up;
    } else if (fields[4][0] == 'd') {
        dir_value = direction::down;
    } else {
        dir_value = direction::unchanged;
    }

    // Parse timestamp - needs conversion from Windows ticks to Unix time
    // Windows ticks are 100-nanosecond intervals since January 1, 0001
    // Unix time is seconds since January 1, 1970
    // Difference is 621,355,968,000,000,000 ticks
    constexpr int64_t WINDOWS_TICKS_TO_UNIX_EPOCH = 621355968000000000LL;
    constexpr int64_t TICKS_PER_NANOSECOND = 100; // 100 ns per tick

    int64_t windows_ticks = std::stoll(fields[11]);

    // Convert Windows ticks to Unix epoch time in nanoseconds
    int64_t unix_time_ns =
        (windows_ticks - WINDOWS_TICKS_TO_UNIX_EPOCH) * TICKS_PER_NANOSECOND;

    auto timestamp_value = std::chrono::time_point<std::chrono::system_clock,
                                                   std::chrono::nanoseconds>{
        std::chrono::nanoseconds(unix_time_ns)};

    auto now =
        std::chrono::system_clock::now(); // Ensure system clock is in UTC
    auto latency_value = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now - timestamp_value);

    // Create tick using aggregate initialization with designated initializers
    tick result{.quote_id = std::stoi(fields[0]),
                .bid = std::stod(fields[1]),
                .ask = std::stod(fields[2]),
                .daily_change = std::stod(fields[3]),
                .dir = dir_value,
                .tradable = fields[5] == "1",
                .high = std::stod(fields[6]),
                .low = std::stod(fields[7]),
                .hash = fields[8],
                .call_only = fields[9] == "1",
                .mid_price = std::stod(fields[10]),
                .timestamp = timestamp_value,
                .field13 = std::stoi(fields[12]),
                .group = price_type,
                .latency = latency_value};

    return result;
}
} // namespace td365
