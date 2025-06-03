/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */


#pragma once

#include <spdlog/spdlog.h>
#include <string>
#include <stdexcept>
#include <format>

namespace td365 {

template <typename... FmtArgs>
std::runtime_error fail(std::string_view msg, FmtArgs... args) {
  spdlog::error(msg, std::forward<FmtArgs>(args)...);

  return std::runtime_error{
    std::format(msg, std::forward<FmtArgs>(args)...)};
}

template <typename... FmtArgs>
void verify(bool condition, std::string_view msg, FmtArgs... args) {
  if (!condition) {
    [[unlikely]] throw fail(msg, std::forward<FmtArgs>(args)...);
  }
}
}
