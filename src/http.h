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

typedef std::unordered_multimap<boost::beast::http::field, const std::string>
    headers;
typedef boost::beast::http::response<boost::beast::http::string_body> response;
typedef boost::beast::http::request<boost::beast::http::string_body> request;

#endif // HTTP_H
