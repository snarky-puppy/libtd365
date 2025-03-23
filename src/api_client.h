/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#ifndef API_CLIENT_H
#define API_CLIENT_H

#include "http_client.h"
#include "td365.h"
#include <string>
#include <vector>

class api_client {
public:
  explicit api_client(std::string_view host);

  // login returns a token used to authenticate the websocket
  std::string login(std::string path);

  void update_session_token();

  std::vector<market_group> get_market_super_group();

  std::vector<market_group>
  get_market_group(unsigned int id);

  std::vector<market> get_market_quote(unsigned int id);

private:
  http_client client_;

  std::string connect(std::string path);
};

#endif // API_CLIENT_H
