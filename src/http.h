/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#pragma once

#include <boost/asio.hpp>
#include <boost/beast/http.hpp>

namespace td365 {
using http_response =
    boost::beast::http::response<boost::beast::http::dynamic_body>;
using http_request =
    boost::beast::http::request<boost::beast::http::string_body>;
using http_headers = std::unordered_multimap<std::string, std::string>;
} // namespace td365
