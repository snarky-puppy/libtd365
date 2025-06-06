/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#pragma once

#include "http_client.h"
#include "nlohmann/json_fwd.hpp"

#include <boost/asio/ssl/context.hpp>
#include <boost/url/url.hpp>

namespace td365 {
nlohmann::json extract_jwt_payload(const nlohmann::json &jwt);

boost::asio::ssl::context &ssl_ctx();

std::string now_utc();

boost::urls::url check_proxy_url();

std::string get_http_body(http_response const &res);

} // namespace td365
