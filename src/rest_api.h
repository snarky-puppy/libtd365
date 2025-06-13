/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#pragma once

#include <boost/asio/awaitable.hpp>
#include <boost/url/url.hpp>
#include <string>
#include <vector>

namespace td365 {

struct http_client;
struct market;
struct market_group;

using boost::asio::awaitable;

class rest_api : public std::enable_shared_from_this<rest_api> {
  public:
    struct auth_info {
        std::string token;
        std::string login_id;
    };

    enum session_token_response { RETRY, FAILURE, LOGOUT, OK };

    explicit rest_api();
    ~rest_api();

    // `connect` simulates opening the web client page. Returns a token used to
    // authenticate the websocket
    auto connect(boost::urls::url) -> awaitable<auth_info>;

    auto get_market_super_group() -> awaitable<std::vector<market_group>>;

    auto get_market_group(int id) -> awaitable<std::vector<market_group>>;

    auto get_market_quote(int id) -> awaitable<std::vector<market>>;

  private:
    std::unique_ptr<http_client> client_;

    auto open_client(std::string_view target, int depth = 0)
        -> awaitable<std::pair<std::string, std::string>>;
};
} // namespace td365
