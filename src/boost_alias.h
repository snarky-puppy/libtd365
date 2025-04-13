/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */


#ifndef BOOST_ALIAS_H
#define BOOST_ALIAS_H

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

namespace beast = boost::beast;
namespace http  = beast::http;
namespace asio  = boost::asio;
namespace ssl   = boost::asio::ssl;
using tcp = asio::ip::tcp;
using boost::asio::awaitable;

#endif //BOOST_ALIAS_H
