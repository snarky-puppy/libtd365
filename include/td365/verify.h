/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#pragma once

#include <format>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>

namespace td365 {

template <typename... FmtArgs>
std::runtime_error fail(std::format_string<FmtArgs...> fmt, FmtArgs... args) {
    std::string msg = std::format(fmt, std::forward<FmtArgs>(args)...);
    spdlog::error(msg);
    return std::runtime_error{msg};
}

template <typename... FmtArgs>
void verify(bool condition, std::format_string<FmtArgs...> fmt,
            FmtArgs... args) {
    if (!condition) {
        [[unlikely]] throw fail(fmt, std::forward<FmtArgs>(args)...);
    }
}
} // namespace td365
