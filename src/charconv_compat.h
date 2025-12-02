/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#pragma once

#ifdef HAVE_STD_FROM_CHARS
#include <charconv>
#else
#include <boost/charconv.hpp>
#endif

#include <string_view>
#include <system_error>

namespace td365 {
namespace charconv_compat {

#ifdef HAVE_STD_FROM_CHARS

template <typename T>
inline std::from_chars_result from_chars(const char *first, const char *last,
                                         T &value) {
    return std::from_chars(first, last, value);
}

#else

template <typename T>
inline boost::charconv::from_chars_result
from_chars(const char *first, const char *last, T &value) {
    return boost::charconv::from_chars(first, last, value);
}

#endif

} // namespace charconv_compat
} // namespace td365
