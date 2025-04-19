/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#ifndef UTILS_H
#define UTILS_H

#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>

#include "execution_ctx.h"

#include "nlohmann/json_fwd.hpp"

nlohmann::json extract_jwt_payload(const nlohmann::json &jwt);

struct splitted_url {
    std::string host;
    std::string path;
};

splitted_url split_url(const std::string &url);

boost::asio::ssl::context &ssl_ctx();

std::string now_utc();

boost::asio::awaitable<boost::asio::ip::tcp::resolver::results_type>
td_resolve(td_context_view ctx,
           const std::string &host, const std::string &port);

boost::asio::ip::tcp::resolver::results_type
td_resolve_sync(boost::asio::io_context &io_context,
                const std::string &host, const std::string &port);

#endif
