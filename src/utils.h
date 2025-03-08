/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#ifndef UTILS_H
#define UTILS_H

#include "nlohmann/json_fwd.hpp"
#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>

nlohmann::json extract_jwt_payload(const nlohmann::json &jwt);

struct splitted_url {
  std::string host;
  std::string path;
};

splitted_url split_url(const std::string &url);

extern boost::asio::ssl::context ssl_ctx;

std::string now_utc();

boost::asio::awaitable<boost::asio::ip::tcp::resolver::results_type>
td_resolve(const boost::asio::any_io_executor &executor,
           const std::string &host, const std::string &port);

#endif
