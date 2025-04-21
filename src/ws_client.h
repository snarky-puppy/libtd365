/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#ifndef WS_CLIENT_H
#define WS_CLIENT_H

#include <atomic>
#include <future>
#include <string>
#include <vector>

#include <boost/asio.hpp>

#include <nlohmann/json_fwd.hpp>

#include "execution_ctx.h"
#include "ws.h"

class ws_client {
public:
    explicit ws_client(td_context_view ctx);

    ~ws_client();

    boost::asio::awaitable<void> connect(const std::string &host);

    void start_loop(std::string host, std::string login_id, std::string token);

    boost::asio::awaitable<void> send(const nlohmann::json &);

    boost::asio::awaitable<void> close();

    boost::asio::awaitable<void> subscribe(int quote_id);

    boost::asio::awaitable<void> unsubscribe(int quote_id);

    std::vector<tick> get_price_data(bool blocking);

    boost::asio::awaitable<void> reconnect(const std::string &host);

    bool connected() { return connected_; }

    void wait_for_auth();

private:
    void process_subscribe_response(const nlohmann::json &msg);

    boost::asio::awaitable<void> process_reconnect_response(const nlohmann::json &msg);

    boost::asio::awaitable<void> process_messages(const std::string &login_id,
                                                  const std::string &token);

    boost::asio::awaitable<void> process_heartbeat(const nlohmann::json &msg);

    boost::asio::awaitable<void>
    process_connect_response(const nlohmann::json &msg,
                             const std::string &login_id,
                             const std::string &token);

    boost::asio::awaitable<void>
    process_authentication_response(const nlohmann::json &msg);

    void process_price_data(const nlohmann::json &msg);

    td_context_view ctx_;
    std::unique_ptr<ws> ws_;
    std::string supported_version_ = "1.0.0.6";
    std::promise<void> auth_promise_;
    std::future<void> auth_future_;
    td_user_context usr_ctx_;

    // Connection state tracking
    std::atomic<bool> connected_{false};
    std::promise<void> disconnect_promise_;
    std::future<void> disconnect_future_;
    std::string connection_id_;
    std::vector<int> subscribed_;
};

#endif // WS_CLIENT_H
