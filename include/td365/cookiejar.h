/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#pragma once

#include <boost/url.hpp>
#include <chrono>
#include <string>
#include <td365/http.h>
#include <unordered_map>

namespace td365 {
class cookiejar {
  public:
    explicit cookiejar(std::string file_name);

    void save() const;

    void update(const http_response &res);

    void apply(http_request &req);

    struct cookie {
        std::string name;
        std::string value;
        // A default-constructed expiry_time indicates a session cookie.
        std::chrono::system_clock::time_point expiry_time;
    };

    cookie get(const std::string &name) const;

  private:
    const std::string path_;
    std::unordered_map<std::string, cookie> cookies_;
};
} // namespace td365
