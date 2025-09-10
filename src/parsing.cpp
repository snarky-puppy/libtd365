/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#include <boost/charconv.hpp>
#include <charconv>
#include <iomanip>
#include <iostream>
#include <ranges>
#include <sstream>
#include <string>
#include <td365/parsing.h>
#include <td365/types.h>
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

template <typename T> T parse(std::string_view sv) {
    T value{};
    auto [ptr, ec] =
        boost::charconv::from_chars(sv.data(), sv.data() + sv.size(), value);
    verify(ec == std::errc(), "bad parse: {}", sv);
    return value;
}

int parse_int(std::string_view sv) { return parse<int>(sv); }
double parse_double(std::string_view sv) { return parse<double>(sv); }

tick parse_td_tick(std::string_view price_string, grouping price_type) {
    constexpr size_t EXPECTED_FIELDS = 13;
    std::array<std::string_view, EXPECTED_FIELDS> fields;
    size_t idx = 0;

    // split on ',' into subranges
    for (auto sub : price_string | std::views::split(',') |
                        std::views::transform([](auto rng) {
                            auto first = rng.begin();
                            auto len =
                                std::ranges::size(rng); // unsigned size_t
                            return std::string_view(&*first, len);
                        })) {
        if (idx < EXPECTED_FIELDS) {
            fields[idx++] = sub;
        } else {
            break;
        }
    }
    verify(idx == EXPECTED_FIELDS, "Invalid price data format: {}",
           price_string);

    // parse direction
    char d0 = fields[4].empty() ? '?' : fields[4][0];
    direction dir_value = (d0 == 'u'   ? direction::up
                           : d0 == 'd' ? direction::down
                                       : direction::unchanged);

    // timestamp conversion
    constexpr int64_t WINDOWS_TICKS_TO_UNIX_EPOCH = 621355968000000000LL;
    constexpr int64_t TICKS_PER_NANOSECOND = 100; // 100 ns

    int64_t windows_ticks{};
    {
        auto [ptr, ec] = std::from_chars(fields[11].data(),
                                         fields[11].data() + fields[11].size(),
                                         windows_ticks);
        if (ec != std::errc())
            throw fail("Bad ticks: ", std::string(fields[11]));
    }
    int64_t unix_ns =
        (windows_ticks - WINDOWS_TICKS_TO_UNIX_EPOCH) * TICKS_PER_NANOSECOND;
    auto timestamp_value = std::chrono::time_point<std::chrono::system_clock,
                                                   std::chrono::nanoseconds>{
        std::chrono::nanoseconds{unix_ns}};
    auto latency_value = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now() - timestamp_value);

    // build and return
    return tick{.quote_id = parse_int(fields[0]),
                .bid = parse_double(fields[1]),
                .ask = parse_double(fields[2]),
                .daily_change = parse_double(fields[3]),
                .dir = dir_value,
                .tradable = (fields[5] == "1"),
                .high = parse_double(fields[6]),
                .low = parse_double(fields[7]),
                .hash = std::string(fields[8]), // needs owning string
                .call_only = (fields[9] == "1"),
                .mid_price = parse_double(fields[10]),
                .timestamp = timestamp_value,
                .field13 = parse_int(fields[12]),
                .group = price_type,
                .latency = latency_value};
}

auto parse_iso8601_sv(std::string_view sv) -> candle::time_type {
    if (sv.size() != 25 || (sv[19] != '+' && sv[19] != '-'))
        throw std::invalid_argument(
            "Wrong format, expected YYYY-MM-DDThh:mm:ss±HH:MM");

    auto to_int = [&](size_t pos, size_t len) {
        return parse_int(sv.substr(pos, len));
    };

    // note the complete lack of error handling. YOLO.
    std::tm tm{};
    tm.tm_year = to_int(0, 4) - 1900;
    tm.tm_mon = to_int(5, 2) - 1;
    tm.tm_mday = to_int(8, 2);
    tm.tm_hour = to_int(11, 2);
    tm.tm_min = to_int(14, 2);
    tm.tm_sec = to_int(17, 2);

    // Convert to time_t (UTC)
    // timegm is GNU extension; on Windows use _mkgmtime
    std::time_t tt = timegm(&tm);

    // Parse timezone offset
    int sign = (sv[19] == '-') ? -1 : +1;
    int off_h = to_int(20, 2);
    int off_m = to_int(23, 2);
    int offset_seconds = sign * (off_h * 3600 + off_m * 60);

    // Adjust to UTC
    tt -= offset_seconds;

    return std::chrono::time_point_cast<std::chrono::seconds>(
        std::chrono::system_clock::from_time_t(tt));
}

candle parse_candle(std::string_view candle_string) {
    constexpr size_t EXPECTED_FIELDS = 6;
    std::array<std::string_view, EXPECTED_FIELDS> fields;
    size_t idx = 0;

    // "2025-06-16T07:32:00+00:00,107109.5,107155.5,107109.5,107128.5,29"
    // 0. 2025-06-16T07:32:00+00:00
    // 1. 107109.5
    // 2. 107155.5
    // 3. 107109.5
    // 4. 107128.5
    // 5. 29

    // split on ',' into subranges
    for (auto sub : candle_string | std::views::split(',') |
                        std::views::transform([](auto rng) {
                            auto first = rng.begin();
                            auto len =
                                std::ranges::size(rng); // unsigned size_t
                            return std::string_view(&*first, len);
                        })) {
        if (idx < EXPECTED_FIELDS) {
            fields[idx++] = sub;
        } else {
            break;
        }
    }
    verify(idx == EXPECTED_FIELDS, "Invalid chart data format: {}",
           candle_string);

    return candle{
        .timestamp = parse_iso8601_sv(fields[0]),
        .open = parse_double(fields[1]),
        .high = parse_double(fields[2]),
        .low = parse_double(fields[3]),
        .close = parse_double(fields[4]),
        .volume = parse_double(fields[5]),
    };
}
} // namespace td365
