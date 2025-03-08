/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#include "utils.h"
#include "base64.hpp"
#include "nlohmann/json.hpp"
#include <boost/asio/ssl.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <charconv>
#include <iostream>
#include <regex>

using json = nlohmann::json;

boost::asio::ssl::context ssl_ctx =
    boost::asio::ssl::context(boost::asio::ssl::context::tlsv12_client);

std::string now_utc() {
  boost::posix_time::ptime now =
      boost::posix_time::second_clock::universal_time();
  return boost::posix_time::to_iso_string(now);
}

boost::asio::awaitable<boost::asio::ip::tcp::resolver::results_type>
td_resolve(const boost::asio::any_io_executor &executor,
           const std::string &host, const std::string &port) {
  auto resolved_host = std::string_view{host};
  auto resolved_port = std::string_view{port};
  if (auto *env = std::getenv("PROXY")) {
    std::string_view proxy{env};
    auto r = proxy | std::ranges::views::split(':');
    auto iter = r.begin();
    resolved_host = std::string_view(*iter);
    iter++;
    resolved_port = std::string_view{"8080"};
    if (iter != r.end()) {
      resolved_port = std::string_view(*iter);
    }
  }
  boost::asio::ip::tcp::resolver resolver(executor);
  auto endpoints = co_await resolver.async_resolve(resolved_host, resolved_port,
                                                   boost::asio::use_awaitable);
  co_return endpoints;
}

json extract_jwt_payload(const json &jwt) {
  if (!jwt.is_string()) {
    throw std::runtime_error(jwt.dump());
  }

  // https://en.cppreference.com/w/cpp/ranges
  auto jwt_range = to_string(jwt) | std::ranges::views::split('.') |
                   std::ranges::views::drop(1) | std::ranges::views::take(1);

  auto it = jwt_range.begin();
  if (it == jwt_range.end()) {
    throw std::runtime_error(jwt.dump());
  }

  // nholmann does not support string_views yet
  std::string payload((*it).begin(), (*it).end());

  while ((payload.size() & 3) != 0) {
    payload.push_back('=');
  }

  const auto decoded = base64::decode_into<std::string>(payload);
  const auto rv = json::parse(decoded);
  return rv; // see NVRO
}

std::string_view trim(const std::string &body) {
  static const char *whitespace = " \t\n\r\f\v";
  std::string_view trimmed{body};
  trimmed.remove_prefix(trimmed.find_first_not_of(whitespace));
  trimmed.remove_suffix(trimmed.find_last_not_of(whitespace));
  return trimmed;
}

splitted_url split_url(const std::string &url) {
  // parse out protocol, hostname, and the path and query
  const static std::regex re(R"(^https://([^/]*)(.*))");
  std::smatch m;
  assert(std::regex_match(url, m, re));
  auto host = m[1].str();
  auto path = m[2].str();
  return splitted_url{host, path};
}
