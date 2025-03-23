/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#ifndef AUTHENTICATOR_H
#define AUTHENTICATOR_H

#include "utils.h"
#include <string>

typedef enum { demo, prod } account_type_t;

struct account_detail {
    splitted_url platform_url;
    std::string login_id;
    account_type_t account_type;
    std::string site_host;
    std::string api_host;
    std::string sock_host;
};

namespace authenticator {
    account_detail authenticate(std::string username, std::string password,
                                std::string account_id);

    account_detail authenticate();
};

#endif // AUTHENTICATOR_H
