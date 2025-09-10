/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#pragma once

#include <boost/asio/awaitable.hpp>
#include <string>
#include <td365/utils.h>

namespace td365 {
typedef enum { demo, prod, oneclick } account_type_t;

struct web_detail {
    boost::urls::url platform_url;
    account_type_t account_type;
    boost::urls::url site_host;
    boost::urls::url api_host;
    boost::urls::url sock_host;
};

namespace authenticator {
boost::asio::awaitable<web_detail> authenticate(std::string username,
                                                std::string password,
                                                std::string account_id);

boost::asio::awaitable<web_detail> authenticate();
}; // namespace authenticator
} // namespace td365
