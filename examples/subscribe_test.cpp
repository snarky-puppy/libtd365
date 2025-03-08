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

void print_usage(const char *program) {
  std::cerr << "Usage: " << program << " [options]\n"
            << "Options:\n"
            << "  --username <username>  : TD365 username\n"
            << "  --password <password>  : TD365 password\n"
            << "  --account <account_id> : TD365 account ID\n"
            << "  --help                 : Show this help message\n"
            << std::endl;
}

int main(int argc, char **argv) {
  std::string username;
  std::string password;
  std::string account_id;

  // Parse command line arguments
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "--help") {
      print_usage(argv[0]);
      return 0;
    } else if (arg == "--username" && i + 1 < argc) {
      username = argv[++i];
    } else if (arg == "--password" && i + 1 < argc) {
      password = argv[++i];
    } else if (arg == "--account" && i + 1 < argc) {
      account_id = argv[++i];
    } else {
      std::cerr << "Unknown option: " << arg << std::endl;
      print_usage(argv[0]);
      return 1;
    }
  }

  // Check for required arguments
  if (username.empty() || password.empty() || account_id.empty()) {
    std::cerr << "Error: Username, password, and account ID are required"
              << std::endl;
    print_usage(argv[0]);
    return 1;
  }

  try {
    td365 client([](const tick &t) { std::cout << t << std::endl; });

    client.connect(username, password, account_id);

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
    client.wait_for_disconnect();

  } catch (const std::exception &e) {
    std::cerr << "terminating: " << e.what() << std::endl;
  } catch (...) {
    std::cerr << "unknown exception" << std::endl;
  }
  return 0;
}
