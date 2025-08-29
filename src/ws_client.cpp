/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#include <td365/ws_client.h>

#include <td365/parsing.h>
#include <td365/td365.h>
#include <td365/utils.h>
#include <td365/ws.h>

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

namespace td365 {

namespace net = boost::asio;
using net::use_awaitable;
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
    account_details
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
        {"accountDetails", payload_type::account_details}};

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

ws_client::ws_client(const user_callbacks &callbacks)
    : callbacks_(callbacks), auth_f_(auth_p_.get_future()) {}

ws_client::~ws_client() {
    // Ensure that any ongoing future operations are cancelled
    // This is important for the auth_f_ future that might be waited on
    try {
        if (auth_f_.valid() && auth_f_.wait_for(std::chrono::seconds(0)) ==
                                   std::future_status::timeout) {
            // If the promise hasn't been set, set it to avoid blocking
            auth_p_.set_value();
        }
    } catch (const std::future_error &) {
        // Promise already set or moved, ignore
    } catch (...) {
        // Ignore other exceptions during cleanup
    }

    // The ws_ unique_ptr will automatically clean up when destroyed
    // This is safe because the destructor is only called after all
    // coroutines using this object have completed
}

boost::asio::awaitable<void> ws_client::connect(boost::urls::url_view u) {
    spdlog::info("ws_client: connecting to {}", u.buffer());
    ws_ = std::make_unique<ws>();
    return ws_->connect(u);
}

boost::asio::awaitable<void> ws_client::subscribe(int quote_id) {
    if (std::ranges::find(subscribed_, quote_id) ==
        std::ranges::end(subscribed_)) {
        subscribed_.push_back(quote_id);
        co_await send({{"quoteId", quote_id},
                       {"priceGrouping", "Sampled"},
                       {"action", "subscribe"}});
    }
    co_return;
}

boost::asio::awaitable<void> ws_client::unsubscribe(int quote_id) {
    auto pos = std::ranges::find(subscribed_, quote_id);
    if (pos != std::ranges::end(subscribed_)) {
        subscribed_.erase(pos);
        co_await send({{"quoteId", quote_id},
                       {"priceGrouping", "Sampled"},
                       {"action", "unsubscribe"}});
    }
    co_return;
}

boost::asio::awaitable<void> ws_client::send(const nlohmann::json &body) {
    co_await ws_->send(body.dump());
    co_return;
}

boost::asio::awaitable<void>
ws_client::message_loop(const std::string &login_id, const std::string &token,
                        std::atomic<bool> &shutdown) {
    while (!shutdown) {
        auto [ec, buf] = co_await ws_->read_message();
        if (ec) {
            spdlog::error("ws_client::message_loop: read_message failed: {}",
                          ec.message());
            if (is_error_continuable(ec)) {
                // FIXME
                auth_p_ = std::promise<void>();
                auth_f_ = auth_p_.get_future();
                co_return;
            }
            spdlog::info("ws_client::message_loop: not continuable");
            throw ec;
        }

        auto msg = nlohmann::json::parse(buf);

        switch (string_to_payload_type(msg["t"].get<std::string>())) {
        case payload_type::connect_response:
            co_await process_connect_response(msg, login_id, token);
            break;
        case payload_type::reconnect_response:
            co_await process_reconnect_response(msg);
            break;
        case payload_type::heartbeat:
            co_await process_heartbeat(msg);
            break;
        case payload_type::authentication_response:
            co_await process_authentication_response(msg);
            break;
        case payload_type::subscribe_response:
            process_subscribe_response(msg);
            break;
        case payload_type::price_data:
            process_price_data(msg);
            break;
        case payload_type::account_summary:
            process_account_summary(msg);
            break;
        case payload_type::account_details:
            process_account_details(msg);
            break;
        default:
            std::cerr << "Unhandled message" << msg.dump() << std::endl;
        }
    }
    std::cout << "ws_client exiting" << std::endl;
    co_return;
}

boost::asio::awaitable<void>
ws_client::process_heartbeat(const nlohmann::json &j) {
    auto now = now_utc();
    co_await send({
        {"SentByServer", j["d"]["SentByServer"]},
        {"MessagesReceived", j["d"]["MessagesReceived"]},
        {"PricesReceived", j["d"]["PricesReceived"]},
        {"MessagesSent", j["d"]["MessagesSent"]},
        {"PricesSent", j["d"]["PricesSent"]},
        {"Visible", true},
        {"action", "heartbeat"},
    });
    co_return;
}

boost::asio::awaitable<void>
ws_client::process_reconnect_response(const nlohmann::json &msg) {
    connection_id_ = msg["cid"].get<std::string>();
    co_return;
}

boost::asio::awaitable<void>
ws_client::process_connect_response(const nlohmann::json &,
                                    const std::string &login_id,
                                    const std::string &token) {
    co_await send({{"action", "authentication"},
                   {"loginId", login_id},
                   {"tradingAccountType", "SPREAD"},
                   {"token", token},
                   {"reason", "Connect"},
                   {"clientVersion", supported_version_}});
    co_return;
}

boost::asio::awaitable<void>
ws_client::process_authentication_response(const nlohmann::json &msg) {
    if (!msg["d"]["Result"].get<bool>()) {
        throw std::runtime_error("Authentication failed");
    }
    if (!connection_id_.empty()) {
        co_await send({{"action", "reconnect"},
                       {"originalConnectionId", connection_id_}});
    }
    connection_id_ = msg["cid"].get<std::string>();

    // subscribe to account summary
    // nb, no constexpr json yet
    co_await send({{"data", "{\"SubscribeToAccountSummary\":true,"
                            "\"SubscribeToAccountDetails\":true}"},
                   {"action", "options"}});

    // re-establish previous quote subscriptions
    for (auto quote_id : subscribed_) {
        co_await send({{"quoteId", quote_id},
                       {"priceGrouping", "Sampled"},
                       {"action", "subscribe"}});
    }
    auth_p_.set_value();
    co_return;
}

void ws_client::process_price_data(const nlohmann::json &msg) {
    const auto &data = msg["d"];

    for (const auto &key : grouping_map) {
        if (auto it = data.find(key.first);
            it != data.end() && it->is_array() && !it->empty()) {
            auto prices = it->get<std::vector<std::string>>();
            for (const auto &price : prices) {
                callbacks_.tick_cb(parse_tick2(price, key.second));
            }
        }
    }
}

void ws_client::process_subscribe_response(const nlohmann::json &msg) {
    auto d = msg["d"];
    verify(d["HasError"].get<bool>() == false, "HasError is true");
    auto prices = d["Current"].get<std::vector<std::string>>();
    auto g = string_to_price_type(d["PriceGrouping"].get<std::string>());
    for (const auto &p : prices) {
        callbacks_.tick_cb(parse_tick(p, g));
    }
}

void ws_client::process_account_summary(const nlohmann::json &msg) {
    spdlog::info("account summary received: {}", msg.dump());
    if (msg.at("d").at("PlatformID").get<int>() == 0) {
        spdlog::info("account summary: skip platform 0:", msg.dump());
        return;
    }
    auto summary = msg["d"].get<account_summary>();
    callbacks_.acc_summary_cb(std::move(summary));
}

void ws_client::process_account_details(const nlohmann::json &msg) {
    spdlog::info("account details received: {}", msg.dump());
    auto details = msg["d"].get<account_details>();
    callbacks_.acc_detail_cb(std::move(details));
}

void ws_client::wait_for_auth() { auth_f_.get(); }

boost::asio::awaitable<void> ws_client::run(boost::urls::url_view url,
                                            const std::string &login_id,
                                            const std::string &token,
                                            std::atomic<bool> &shutdown) {
    while (!shutdown.load()) {
        co_await connect(url);
        co_await message_loop(login_id, token, shutdown);
    }
}

} // namespace td365
