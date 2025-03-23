/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#ifndef WS_CLIENT_H
#define WS_CLIENT_H

#include "td365.h"
#include "ws.h"
#include <atomic>
#include <boost/asio.hpp>
#include <condition_variable>
#include <functional>
#include <future>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <vector>

enum class grouping;
class portal;

class ws_client {
public:
    explicit ws_client(
        const boost::asio::any_io_executor &executor,
        const std::atomic<bool> &shutdown,
        std::function<void(const tick &)> &&tick_callback = nullptr);

    ~ws_client();

    boost::asio::awaitable<void> connect(const std::string &host);

    void start_loop(std::string host, std::string login_id, std::string token);

    boost::asio::awaitable<void> send(const nlohmann::json &);

    boost::asio::awaitable<void> close();

    boost::asio::awaitable<void> subscribe(int quote_id);

    boost::asio::awaitable<void> unsubscribe(int quote_id);

    std::vector<tick> get_price_data(bool blocking);

    // Block until the WebSocket connection is closed
    void wait_for_disconnect();

    boost::asio::awaitable<void> reconnect();

private:
    void process_subscribe_response(const nlohmann::json &msg);

    boost::asio::awaitable<::boost::beast::error_code> process_messages(const std::string &login_id,
                                                                        const std::string &token);

    boost::asio::awaitable<void> process_heartbeat(const nlohmann::json &msg);

    boost::asio::awaitable<void>
    process_connect_response(const nlohmann::json &msg,
                             const std::string &login_id,
                             const std::string &token);

    boost::asio::awaitable<void>
    process_authentication_response(const nlohmann::json &msg);

    void deliver_tick(tick &&t);

    void push_data(std::vector<tick> &&);

    void process_price_data(const nlohmann::json &msg);

    boost::asio::any_io_executor executor_;
    std::unique_ptr<ws> ws_;
    const std::atomic<bool> &shutdown_;
    std::string supported_version_ = "1.0.0.6";
    std::promise<void> auth_promise_;
    std::future<void> auth_future_;
    std::function<void(const tick &)> tick_callback_;

    // Connection state tracking
    std::atomic<bool> connected_{false};
    std::promise<void> disconnect_promise_;
    std::future<void> disconnect_future_;
};

#endif // WS_CLIENT_H
