/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#include "td365.h"
#include "platform.h"
#include <iostream>

td365::td365(tick_callback callback)
    : platform_(std::make_unique<platform>(std::move(callback))) {}

// Define destructor here to ensure complete definition of platform is available
td365::~td365() = default;

void td365::connect(const std::string &username, const std::string &password,
                    const std::string &account_id) {
  platform_->connect(username, password, account_id);
}

std::vector<market_group> td365::get_market_super_group() const {
  return platform_->get_market_super_group();
}

std::vector<market_group> td365::get_market_group(int id) const {
  return platform_->get_market_group(id);
}

std::vector<market> td365::get_market_quote(int id) const {
  return platform_->get_market_quote(id);
}

void td365::subscribe(int quote_id) const { platform_->subscribe(quote_id); }

void td365::wait_for_disconnect() const { platform_->wait_for_disconnect(); }
