/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#pragma once

#include <atomic>
#include <boost/asio.hpp>
#include <boost/url/url.hpp>
#include <boost/url/url_view.hpp>
#include <chrono>
#include <future>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <td365/types.h>
#include <td365/ws.h>
#include <vector>

namespace td365 {

class ws_client {
  public:
    explicit ws_client();

    ~ws_client();

    void connect(boost::urls::url_view url, const std::string &login_id,
                 const std::string &token);

    event
    read_and_process_message(std::optional<std::chrono::milliseconds> timeout);

    void send(const nlohmann::json &);

    void subscribe(int quote_id);

    void unsubscribe(int quote_id);

  private:
    std::optional<event> process_subscribe_response(const nlohmann::json &msg);

    std::optional<event> process_reconnect_response(const nlohmann::json &msg);

    std::optional<event> process_heartbeat(const nlohmann::json &msg);

    std::optional<event> process_connect_response(const nlohmann::json &msg,
                                                  const std::string &login_id,
                                                  const std::string &token);

    std::optional<event>
    process_authentication_response(const nlohmann::json &msg);

    event process_price_data(const nlohmann::json &msg);
    event process_account_summary(const nlohmann::json &msg);
    event process_account_details(const nlohmann::json &msg);
    event process_trade_established(const nlohmann::json &msg);

    std::unique_ptr<ws> ws_;
    std::string supported_version_ = "1.0.0.6";
    std::string login_id_;
    std::string token_;

    // Connection state tracking
    std::string connection_id_;
    std::vector<int> subscribed_;

    // Reconnection state
    boost::urls::url stored_url_;
    std::chrono::milliseconds reconnect_delay_ =
        std::chrono::milliseconds(1000);
};
} // namespace td365
