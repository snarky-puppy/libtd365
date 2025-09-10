/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#pragma once

#include <boost/beast/http/status.hpp>
#include <format>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>

enum class api_error {
    ok = 0,
    extract_ots,
    find_login_id,
    parse_login_id,
    login,
    http_post,
    json_parse,
    session_status,
    max_depth,
};

class api_error_category : public std::error_category {
  public:
    const char *name() const noexcept override { return "api_error"; }

    std::string message(int ev) const override {
        switch (static_cast<api_error>(ev)) {
        case api_error::ok:
            return "ok";
        case api_error::extract_ots:
            return "extract_ots";
        case api_error::find_login_id:
            return "find_login_id";
        case api_error::parse_login_id:
            return "parse_login_id";
        case api_error::login:
            return "login";
        case api_error::http_post:
            return "http_post";
        case api_error::json_parse:
            return "json_parse";
        case api_error::session_status:
            return "session_status";
        case api_error::max_depth:
            return "max_depth";
        default:
            std::abort();
        }
    }
};

// register enum as an error code enum
template <> struct std::is_error_code_enum<api_error> : std::true_type {};

inline const std::error_category &get_api_error_category() {
    static api_error_category instance;
    return instance;
}

// Mapping from error code enum to category
inline std::error_code make_error_code(api_error e) {
    return {static_cast<int>(e), get_api_error_category()};
}
inline const char *api_error_message(api_error e) {
    switch (e) {
    case api_error::ok:
        return "Success";
    case api_error::extract_ots:
        return "Failed to extract OTS";
    case api_error::find_login_id:
        return "Login ID not found";
    case api_error::parse_login_id:
        return "Could not parse Login ID";
    case api_error::login:
        return "Login error";
    case api_error::http_post:
        return "HTTP POST failed";
    case api_error::json_parse:
        return "JSON parsing failed";
    case api_error::session_status:
        return "Session status invalid";
    case api_error::max_depth:
        return "Max depth exceeded";
    default:
        return "Unknown error";
    }
}

[[noreturn]] inline void throw_api_error(api_error e,
                                         std::string_view detail = {}) {
    std::string msg = std::string(api_error_message(e));
    if (!detail.empty()) {
        msg += ": ";
        msg += detail;
    }
    spdlog::error("throw_api_error: {}", msg);
    throw std::system_error(make_error_code(e), msg);
}

template <typename... Args>
[[noreturn]] inline void
throw_api_error(api_error e, std::format_string<Args...> fmt, Args &&...args) {
    throw_api_error(e, std::format(fmt, std::forward<Args>(args)...));
}
