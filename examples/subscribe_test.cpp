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
#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>


int main(int argc, char **argv) {
  try {
    td365 client;

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
    client.main_loop([](const tick &t) { std::cout << t << std::endl; });
  } catch (const std::exception &e) {
    std::cerr << "terminating: " << e.what() << std::endl;
  } catch (...) {
    std::cerr << "unknown exception" << std::endl;
  }
  return 0;
}
