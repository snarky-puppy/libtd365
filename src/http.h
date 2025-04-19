/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#ifndef HTTP_H
#define HTTP_H

#include <boost/asio.hpp>
#include <boost/beast/http.hpp>
#include <utility>

using headers = std::unordered_multimap<std::string, std::string>;
using http_response = boost::beast::http::response<boost::beast::http::string_body>;
using response = http_response;
using request = boost::beast::http::request<boost::beast::http::string_body>;

#endif // HTTP_H
