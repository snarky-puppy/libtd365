/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#pragma once

#include "types.h"
#include "ws.h"

#include <atomic>
#include <boost/asio.hpp>
#include <boost/url/url_view.hpp>
#include <future>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <vector>

namespace td365 {

class ws_client {
  public:
    explicit ws_client(const user_callbacks &);

    ~ws_client();

    boost::asio::awaitable<void> connect(boost::urls::url_view);

    boost::asio::awaitable<void> message_loop(const std::string &login_id,
                                              const std::string &token,
                                              std::atomic<bool> &shutdown);

    boost::asio::awaitable<void> send(const nlohmann::json &);

    boost::asio::awaitable<void> close();

    boost::asio::awaitable<void> subscribe(int quote_id);

    boost::asio::awaitable<void> unsubscribe(int quote_id);

    std::vector<tick> get_price_data(bool blocking);

    bool connected() { return connected_; }

    void wait_for_auth();

  private:
    void process_subscribe_response(const nlohmann::json &msg);

    boost::asio::awaitable<void>
    process_reconnect_response(const nlohmann::json &msg);

    boost::asio::awaitable<void> process_heartbeat(const nlohmann::json &msg);

    boost::asio::awaitable<void>
    process_connect_response(const nlohmann::json &msg,
                             const std::string &login_id,
                             const std::string &token);

    boost::asio::awaitable<void>
    process_authentication_response(const nlohmann::json &msg);

    void process_price_data(const nlohmann::json &msg);

    user_callbacks callbacks_;
    std::unique_ptr<ws> ws_;
    std::string supported_version_ = "1.0.0.6";
    std::promise<void> auth_promise_;
    std::future<void> auth_future_;
    user_callbacks usr_ctx_;

    // Connection state tracking
    std::atomic<bool> connected_{false};
    std::promise<void> disconnect_promise_;
    std::future<void> disconnect_future_;
    std::string connection_id_;
    std::vector<int> subscribed_;
};
} // namespace td365
