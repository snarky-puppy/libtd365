/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#ifndef ASYNC_H
#define ASYNC_H

#include <boost/asio.hpp>

template <typename T> using awaitable = boost::asio::awaitable<T>;

#endif // ASYNC_H
