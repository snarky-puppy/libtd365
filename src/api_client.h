/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#pragma once

#include "async.h"
#include "execution_ctx.h"

#include <boost/url/url.hpp>
#include <string>
#include <vector>

class http_client;
struct market;
struct market_group;

class api_client : public std::enable_shared_from_this<api_client> {
public:
  struct auth_info {
    std::string token;
    std::string login_id;
  };

  enum session_token_response { RETRY, FAILURE, LOGOUT, OK };

  explicit api_client();
  ~api_client();

  // `open` simulates opening the web client page. Returns a token used to
  // authenticate the websocket
  awaitable<auth_info> open(boost::urls::url);

  awaitable<void> close();

  // I think that this is some kind of keep-alive as it doesn't return anything
  // useful.
  void start_session_loop();

  awaitable<std::vector<market_group>> get_market_super_group();

  awaitable<std::vector<market_group>> get_market_group(int id);

  awaitable<std::vector<market>> get_market_quote(int id);

private:
  td_context_view ctx_;
  std::unique_ptr<http_client> client_;
  boost::asio::steady_timer timer_;

  awaitable<std::pair<std::string, std::string>>
  open_client(std::string_view path, int depth = 0);
};
