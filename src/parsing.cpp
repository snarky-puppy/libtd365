/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#include "parsing.h"
#include "td365.h"

#include <cassert>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

// Convert direction enum to string for output
std::string_view to_string(direction dir) {
  switch (dir) {
  case direction::up:
    return "up";
  case direction::down:
    return "down";
  case direction::unchanged:
    return "unchanged";
  default:
    return "unknown";
  }
}

// Convert price_type enum to string for output
std::string_view to_string(grouping pt) {
  static const std::unordered_map<grouping, std::string_view> lookup = {
      {grouping::grouped, "Grouped"},
      {grouping::sampled, "Sampled"},
      {grouping::delayed, "Delayed"},
      {grouping::candle_1m, "Candle1Minute"}};

  auto it = lookup.find(pt);
  return (it != lookup.end()) ? it->second : "Unknown";
}

// String to price_type conversion helper
grouping string_to_price_type(std::string_view key) {
  auto it = grouping_map.find(key);
  return (it != grouping_map.end()) ? it->second : grouping::sampled;
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
    std::cerr << "Invalid price data format: " << price_string << std::endl;
    assert(false);
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

  auto now = std::chrono::system_clock::now(); // Ensure system clock is in UTC
  auto latency_value = std::chrono::duration_cast<std::chrono::nanoseconds>(
      now - timestamp_value);

  // Create tick using aggregate initialization with designated initializers
  tick result{.quote_id = std::stoi(fields[0]),
              .bid = std::stod(fields[1]),
              .ask = std::stod(fields[2]),
              .daily_change = std::stod(fields[3]),
              .dir = dir_value,
              .field6 = std::stoi(fields[5]),
              .high = std::stod(fields[6]),
              .low = std::stod(fields[7]),
              .hash = fields[8],
              .field10 = std::stoi(fields[9]),
              .mid_price = std::stod(fields[10]),
              .timestamp = timestamp_value,
              .field13 = std::stoi(fields[12]),
              .grouping = price_type,
              .latency = latency_value};

  return result;
}

// Stream output operator for tick objects
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
     << ", dir: " << to_string(t.dir) << ", high: " << t.high
     << ", low: " << t.low << ", mid: " << t.mid_price
     << ", time: " << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
     << '.' << std::setw(3) << std::setfill('0') << ms.count()
     << ", latency: " << (t.latency.count() / 1000000.0) << "ms"
     << ", type: " << to_string(t.grouping) << " }";
  return os;
}

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
