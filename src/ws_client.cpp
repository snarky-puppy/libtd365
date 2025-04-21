/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#include "ws_client.h"
#include "constants.h"
#include "parsing.h"
#include "td365.h"
#include "utils.h"
#include "ws.h"
#include <boost/asio/detached.hpp>
#include <boost/asio/ssl.hpp>
#include <iostream>
#include <algorithm>
#include <nlohmann/json.hpp>
#include <print>
#include <cstdlib>
#include <ranges>
#include <boost/lexical_cast.hpp>

namespace net = boost::asio;
using net::awaitable;
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

constexpr auto port = "443";

payload_type string_to_payload_type(std::string_view str) {
  static const std::unordered_map<std::string_view, payload_type> lookup = {
    {"heartbeat", payload_type::heartbeat},
    {"connectResponse", payload_type::connect_response},
    {"reconnectResponse", payload_type::reconnect_response},
    {"authenticationResponse", payload_type::authentication_response},
    {"subscribeResponse", payload_type::subscribe_response},
    {"p", payload_type::price_data},
  };

  auto it = lookup.find(str);
  return (it != lookup.end()) ? it->second : payload_type::unknown;
}

bool is_error_continuable(const boost::system::error_code &ec) {
  if (
    ec == boost::asio::error::operation_aborted ||
    ec == boost::beast::websocket::error::closed
  )
    return true;
  return false;
}

ws_client::ws_client(td_context_view ctx)
  : ctx_(ctx)
    , ws_(std::make_unique<ws>(ctx))
    , auth_future_(auth_promise_.get_future())
    , disconnect_future_(disconnect_promise_.get_future()) {
}

ws_client::~ws_client() {
}

void ws_client::start_loop(std::string host, std::string login_id, std::string token) {
  net::co_spawn(
    ctx_.executor,
    [this, token, login_id, host]() -> net::awaitable<void> {
      try {
        co_await connect(host);
        while (!ctx_.is_shutting_down()) {
          co_await process_messages(login_id, token);
          co_await reconnect(host);
        }
        std::println("ws_client: message loop exiting: {}", ctx_.is_shutting_down());
      } catch (const std::exception &e) {
        std::println(std::cerr, "ws_client: {}", e.what());
      } catch (...) {
        std::println(std::cerr, "ws_client: unknown exception");
      }
    },
    net::detached);
}

boost::asio::awaitable<void> ws_client::close() {
  if (connected_) {
    co_await ws_->close();
    connected_ = false;
    disconnect_promise_.set_value();
  }
}

boost::asio::awaitable<void> ws_client::subscribe(int quote_id) {
  if (std::ranges::find(subscribed_, quote_id) == std::ranges::end(subscribed_)) {
    subscribed_.push_back(quote_id);
    co_await send({
      {"quoteId", quote_id},
      {"priceGrouping", "Sampled"},
      {"action", "subscribe"}
    });
  }
  co_return;
}

boost::asio::awaitable<void> ws_client::unsubscribe(int quote_id) {
  auto pos = std::ranges::find(subscribed_, quote_id);
  if (pos != std::ranges::end(subscribed_)) {
    subscribed_.erase(pos);
    co_await send({
      {"quoteId", quote_id},
      {"priceGrouping", "Sampled"},
      {"action", "unsubscribe"}
    });
  }
  co_return;
}

boost::asio::awaitable<void> ws_client::reconnect(const std::string &host) {
  std::cout << "reconnecting..." << std::endl;
  ws_ = std::make_unique<ws>(ctx_);
  co_await ws_->connect(host, port);
  co_return;
}

boost::asio::awaitable<void> ws_client::connect(const std::string &host) {
  co_await ws_->connect(host, port);

  // Mark as connected and reset disconnect promise
  connected_ = true;
  // Create a new promise in case this is a reconnect
  disconnect_promise_ = std::promise<void>();
  disconnect_future_ = disconnect_promise_.get_future();

  co_return;
}

boost::asio::awaitable<void> ws_client::send(const nlohmann::json &body) {
  co_await ws_->send(body.dump());
  co_return;
}

boost::asio::awaitable<void>
ws_client::process_messages(const std::string &login_id,
                            const std::string &token) {
  while (!ctx_.is_shutting_down()) {
    auto [ec, buf] = co_await ws_->read_message();
    if (ec) {
      std::println(std::cerr, "ws_client: error reading message: {}", ec.message());
      if (is_error_continuable(ec)) {
        co_return;
      }
      throw ec;
    }

    auto msg = nlohmann::json::parse(buf);

    switch (string_to_payload_type(msg["t"].get<std::string>())) {
      case payload_type::connect_response:
        co_await process_connect_response(msg, login_id, token);
        break;
      case payload_type::reconnect_response:
        co_await process_reconnect_response(msg);
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
ws_client::process_connect_response(const nlohmann::json &msg,
                                    const std::string &login_id,
                                    const std::string &token) {
  co_await send({
    {"action", "authentication"},
    {"loginId", login_id},
    {"tradingAccountType", "SPREAD"},
    {"token", token},
    {"reason", "Connect"},
    {"clientVersion", supported_version_}
  });
  co_return;
}

boost::asio::awaitable<void>
ws_client::process_authentication_response(const nlohmann::json &msg) {
  if (!msg["d"]["Result"].get<bool>()) {
    throw std::runtime_error("Authentication failed");
  }
  if (!connection_id_.empty()) {
    co_await send({
      {"action", "reconnect"},
      {"originalConnectionId", connection_id_}
    });
  }
  connection_id_ = msg["cid"].get<std::string>();

  // subscribe to account summary
  co_await send({
    {"data", "{\"SubscribeToAccountSummary\":true,\"SubscribeToAccountDetails\":true}"},
    {"action", "options"}
  });

  // re-establish previous quote subscriptions
  for (auto quote_id: subscribed_) {
    co_await send({
      {"quoteId", quote_id},
      {"priceGrouping", "Sampled"},
      {"action", "subscribe"}
    });
  }
  auth_promise_.set_value();
  co_return;
}

void ws_client::process_price_data(const nlohmann::json &msg) {
  const auto &data = msg["d"];

  for (const auto &key: grouping_map) {
    if (auto it = data.find(key.first);
      it != data.end() && it->is_array() && !it->empty()) {
      try {
        auto prices = it->get<std::vector<std::string> >();
        for (const auto &price: prices) {
          ctx_.usr_ctx.on_tick(parse_tick(price, key.second));
        }
      } catch (const nlohmann::json::exception &e) {
        std::cerr << "Error parsing price data: " << e.what() << std::endl;
      }
    }
  }
}

void ws_client::wait_for_auth() {
  auth_future_.get();
}

void ws_client::process_subscribe_response(const nlohmann::json &msg) {
  auto d = msg["d"];
  assert(d["HasError"].get<bool>() == false);
  auto prices = d["Current"].get<std::vector<std::string> >();
  auto g = string_to_price_type(d["PriceGrouping"].get<std::string>());
  for (const auto &p: prices) {
    ctx_.usr_ctx.on_tick(parse_tick(p, g));
  }
}
