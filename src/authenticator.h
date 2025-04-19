/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#ifndef AUTHENTICATOR_H
#define AUTHENTICATOR_H

#include <execution_ctx.h>

#include "utils.h"
#include <boost/asio/awaitable.hpp>
#include <string>

typedef enum { demo, prod, oneclick } account_type_t;

struct account_detail {
    splitted_url platform_url;
    account_type_t account_type;
    std::string site_host;
    std::string api_host;
    std::string sock_host;
};

namespace authenticator {
    boost::asio::awaitable<account_detail> authenticate(td_context_view ctx,
                                                        std::string username,
                                                        std::string password,
                                                        std::string account_id);

    boost::asio::awaitable<account_detail> authenticate();
};

#endif // AUTHENTICATOR_H
