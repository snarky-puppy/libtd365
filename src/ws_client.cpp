/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#include <algorithm>
#include <boost/asio/detached.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/lexical_cast.hpp>
#include <cstdlib>
#include <iostream>
#include <nlohmann/json.hpp>
#include <print>
#include <ranges>
#include <spdlog/spdlog.h>
#include <td365/parsing.h>
#include <td365/td365.h>
#include <td365/utils.h>
#include <td365/ws.h>
#include <td365/ws_client.h>

namespace td365 {
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using json = nlohmann::json;

enum class payload_type {
    heartbeat,
    connect_response,
    reconnect_response,
    authentication_response,
    unknown,
    subscribe_response,
    price_data,
    account_summary,
    account_details,
    trade_established
};

payload_type string_to_payload_type(std::string_view str) {
    static const std::unordered_map<std::string_view, payload_type> lookup = {
        {"heartbeat", payload_type::heartbeat},
        {"connectResponse", payload_type::connect_response},
        {"reconnectResponse", payload_type::reconnect_response},
        {"authenticationResponse", payload_type::authentication_response},
        {"subscribeResponse", payload_type::subscribe_response},
        {"p", payload_type::price_data},
        {"accountSummary", payload_type::account_summary},
        {"accountDetails", payload_type::account_details},
        {"tradeEstablished", payload_type::trade_established}};

    auto it = lookup.find(str);
    return (it != lookup.end()) ? it->second : payload_type::unknown;
}

bool is_error_continuable(const boost::system::error_code &ec) {
    if (ec == boost::asio::error::operation_aborted ||
        ec == boost::beast::websocket::error::closed ||
        ec == boost::asio::ssl::error::stream_truncated)
        return true;
    return false;
}

ws_client::ws_client() {}

ws_client::~ws_client() = default;

void ws_client::connect(boost::urls::url_view url, const std::string &login_id,
                        const std::string &token) {
    spdlog::info("ws_client: connecting to {}", url.buffer());
    login_id_ = login_id;
    token_ = token;
    stored_url_ = url;

    ws_ = std::make_unique<ws>();
    ws_->connect(url);

    // Read connect response
    auto [ec, buf] = ws_->read_message();
    if (ec) {
        throw std::runtime_error("Failed to read connect response: " +
                                 ec.message());
    }

    auto msg = nlohmann::json::parse(buf);
    process_connect_response(msg, login_id, token);

    // Read authentication response
    std::tie(ec, buf) = ws_->read_message();
    if (ec) {
        throw std::runtime_error("Failed to read auth response: " +
                                 ec.message());
    }

    msg = nlohmann::json::parse(buf);
    process_authentication_response(msg);
}

void ws_client::subscribe(int quote_id) {
    if (std::ranges::find(subscribed_, quote_id) ==
        std::ranges::end(subscribed_)) {
        subscribed_.push_back(quote_id);
        send({{"quoteId", quote_id},
              {"priceGrouping", "Sampled"},
              {"action", "subscribe"}});
    }
}

void ws_client::unsubscribe(int quote_id) {
    auto pos = std::ranges::find(subscribed_, quote_id);
    if (pos != std::ranges::end(subscribed_)) {
        subscribed_.erase(pos);
        send({{"quoteId", quote_id},
              {"priceGrouping", "Sampled"},
              {"action", "unsubscribe"}});
    }
}

void ws_client::send(const nlohmann::json &body) { ws_->send(body.dump()); }

event ws_client::read_and_process_message(
    std::optional<std::chrono::milliseconds> timeout) {
    auto start_time = std::chrono::steady_clock::now();
    auto deadline = timeout ? start_time + *timeout
                            : std::chrono::steady_clock::time_point::max();

    while (true) {
        auto now = std::chrono::steady_clock::now();
        auto remaining = deadline - now;

        if (timeout && remaining <= std::chrono::milliseconds(0)) {
            return timeout_event{};
        }

        auto read_timeout =
            timeout ? std::optional(
                          std::chrono::duration_cast<std::chrono::milliseconds>(
                              remaining))
                    : std::nullopt;

        auto [ec, buf] = ws_->read_message(read_timeout);
        if (ec) {
            if (ec == boost::asio::error::operation_aborted ||
                ec == boost::beast::error::timeout) {
                return timeout_event{};
            }
            if (is_error_continuable(ec)) {
                return connection_closed_event{};
            }
            return error_event{ec.message(), std::current_exception()};
        }

        auto msg = nlohmann::json::parse(buf);

        std::optional<event> evt;
        switch (string_to_payload_type(msg["t"].get<std::string>())) {
        case payload_type::connect_response:
            evt = process_connect_response(msg, login_id_, token_);
            break;
        case payload_type::reconnect_response:
            evt = process_reconnect_response(msg);
            break;
        case payload_type::heartbeat:
            evt = process_heartbeat(msg);
            break;
        case payload_type::authentication_response:
            evt = process_authentication_response(msg);
            break;
        case payload_type::subscribe_response:
            evt = process_subscribe_response(msg);
            break;
        case payload_type::price_data:
            return process_price_data(msg);
        case payload_type::account_summary:
            return process_account_summary(msg);
        case payload_type::account_details:
            return process_account_details(msg);
        case payload_type::trade_established:
            return process_trade_established(msg);
        default:
            spdlog::warn("Unhandled message: {}", msg.dump());
        }

        if (evt) {
            return *evt;
        }
    }
}

std::optional<event> ws_client::process_heartbeat(const nlohmann::json &j) {
    send({
        {"SentByServer", j["d"]["SentByServer"]},
        {"MessagesReceived", j["d"]["MessagesReceived"]},
        {"PricesReceived", j["d"]["PricesReceived"]},
        {"MessagesSent", j["d"]["MessagesSent"]},
        {"PricesSent", j["d"]["PricesSent"]},
        {"Visible", true},
        {"action", "heartbeat"},
    });
    return std::nullopt;
}

std::optional<event>
ws_client::process_reconnect_response(const nlohmann::json &msg) {
    connection_id_ = msg["cid"].get<std::string>();
    return std::nullopt;
}

std::optional<event>
ws_client::process_connect_response(const nlohmann::json &,
                                    const std::string &login_id,
                                    const std::string &token) {
    send({{"action", "authentication"},
          {"loginId", login_id},
          {"tradingAccountType", "SPREAD"},
          {"token", token},
          {"reason", "Connect"},
          {"clientVersion", supported_version_}});
    return std::nullopt;
}

std::optional<event>
ws_client::process_authentication_response(const nlohmann::json &msg) {
    if (!msg["d"]["Result"].get<bool>()) {
        throw std::runtime_error("Authentication failed");
    }
    if (!connection_id_.empty()) {
        send({{"action", "reconnect"},
              {"originalConnectionId", connection_id_}});
    }
    connection_id_ = msg["cid"].get<std::string>();

    // subscribe to account summary
    send({{"data", "{\"SubscribeToAccountSummary\":true,"
                   "\"SubscribeToAccountDetails\":true}"},
          {"action", "options"}});

    // re-establish previous quote subscriptions
    for (auto quote_id : subscribed_) {
        send({{"quoteId", quote_id},
              {"priceGrouping", "Sampled"},
              {"action", "subscribe"}});
    }
    return std::nullopt;
}

event ws_client::process_price_data(const nlohmann::json &msg) {
    const auto &data = msg["d"];

    for (const auto &key : grouping_map) {
        if (auto it = data.find(key.first);
            it != data.end() && it->is_array() && !it->empty()) {
            auto prices = it->get<std::vector<std::string>>();
            for (const auto &price : prices) {
                return tick_event{parse_td_tick(price, key.second)};
            }
        }
    }
    throw std::runtime_error("process_price_data: no price data found");
}

std::optional<event>
ws_client::process_subscribe_response(const nlohmann::json &msg) {
    auto d = msg["d"];
    verify(d["HasError"].get<bool>() == false, "HasError is true");
    auto prices = d["Current"].get<std::vector<std::string>>();
    auto g = string_to_price_type(d["PriceGrouping"].get<std::string>());
    if (!prices.empty()) {
        return tick_event{parse_td_tick(prices[0], g)};
    }
    return std::nullopt;
}

event ws_client::process_account_summary(const nlohmann::json &msg) {
    spdlog::info("account summary received: {}", msg.dump());
    // - PlatformID: 0 - Basic/Standard platform
    // - PlatformID: 3 - Platform with Spread/CFD switching capability
    if (msg.at("d").at("PlatformID").get<int>() == 0) {
        spdlog::info("account summary: skip platform 0:", msg.dump());
        // throw std::runtime_error("Skipping platform 0 account summary");
        return account_summary_event{};
    }
    auto summary = msg["d"].get<account_summary>();
    return account_summary_event{std::move(summary)};
}

event ws_client::process_account_details(const nlohmann::json &msg) {
    spdlog::info("account details received: {}", msg.dump());
    auto details = msg["d"].get<account_details>();
    return account_details_event{std::move(details)};
}

event ws_client::process_trade_established(const nlohmann::json &msg) {
    spdlog::info("trade established received: {}", msg.dump());
    auto details = msg["d"].get<trade_details>();
    return trade_established_event{std::move(details)};
}
} // namespace td365
