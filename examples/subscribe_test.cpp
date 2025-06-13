/*
 * The Prosperity Public License 3.0.0
 *
 * Copyright (c) 2025, Matt Wlazlo
 *
 * Contributor: Matt Wlazlo
 * Source Code: https://github.com/snarky-puppy/td365
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#include "td365.h"

#include <algorithm>
#include <boost/core/verbose_terminate_handler.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <cassert>
#include <iostream>
#include <spdlog/spdlog.h>
#include <vector>

int main(int, char **) {
    std::set_terminate(boost::core::verbose_terminate_handler);

    boost::asio::io_context ioc;
    spdlog::set_level(spdlog::level::debug);

    auto ctx = td365::user_callbacks{
        .tick_cb = [&](td365::tick &&t) { std::cout << t << std::endl; },
    };

    td365::td365 client(ctx);

    client.connect();

    auto super_groups = client.get_market_super_group();
    auto crypto = std::ranges::find_if(super_groups, [](const auto &x) {
        return x.name.compare("Cryptocurrency") == 0;
    });
    assert(crypto != super_groups.end());

    auto group = client.get_market_group(crypto->id);
    assert(group.size() == 1);

    auto markets = client.get_market_quote(group[0].id);
    std::ranges::for_each(
        markets, [&client](const auto &x) { client.subscribe(x.quote_id); });

    boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
        work_guard(ioc.get_executor());
    ioc.run();

    return 0;
}
