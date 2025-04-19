/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#ifndef COOKIEJAR_H
#define COOKIEJAR_H

#include "http.h"
#include <chrono>
#include <string>
#include <unordered_map>

class cookiejar {
public:
  explicit cookiejar(std::string path);

  void save() const;

  void update(const http_response &res);

  void apply(request &req);

  struct cookie {
    std::string name;
    std::string value;
    // A default-constructed expiry_time indicates a session cookie.
    std::chrono::system_clock::time_point expiry_time;
  };

  cookie get(const std::string &ots) const;

private:
  std::string path_;
  std::unordered_map<std::string, cookie> cookies_;
};

#endif // COOKIEJAR_H
