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

// Using string_to_price_type from parsing.h

ws_client::ws_client(const net::any_io_executor &executor,
                     const std::atomic<bool> &shutdown,
                     std::function<void(const tick &)> &&tick_callback)
  : executor_(executor)
    , ws_(std::make_unique<ws>(executor))
    , shutdown_(shutdown)
    , auth_future_(auth_promise_.get_future())
    , tick_callback_(std::move(tick_callback))
    , disconnect_future_(disconnect_promise_.get_future()) {
}

ws_client::~ws_client() {
}

void ws_client::start_loop(std::string host, std::string login_id, std::string token) {
  net::co_spawn(
    executor_,
    [this, token, login_id, host]() -> net::awaitable<void> {
      co_await connect(host);
      while (!shutdown_) {
        auto ec = co_await process_messages(login_id, token);
        if (ec)
          if (ec == boost::beast::websocket::error::closed) {
            co_await reconnect(host);
            continue;
          }
        std::println(std::cerr, "ws_client: {} ({})", ec.message(), ec.what());

        // Mark as disconnected and notify any waiting threads
        connected_ = false;
        disconnect_promise_.set_value();

        throw boost::system::system_error(ec);
      }
    },
    net::detached);

  auth_future_.wait();
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
}

void ws_client::wait_for_disconnect() {
  if (connected_) {
    // Wait for the disconnect_future to be ready
    disconnect_future_.wait();
  }
}

boost::asio::awaitable<void> ws_client::reconnect(const std::string &host) {
  std::cout << "reconnecting..." << std::endl;
  ws_.reset(new ws(executor_));
  co_await ws_->connect(host, port);
  for (auto quote_id: subscribed_) {
    co_await send({
      {"quoteId", quote_id},
      {"priceGrouping", "Sampled"},
      {"action", "subscribe"}
    });
  }
  co_return;
}

boost::asio::awaitable<void> ws_client::connect(const std::string &host) {
  co_await ws_->connect(host, port);

  // Mark as connected and reset disconnect promise
  connected_ = true;
  // Create a new promise in case this is a reconnect
  disconnect_promise_ = std::promise<void>();
  disconnect_future_ = disconnect_promise_.get_future();
}

boost::asio::awaitable<void> ws_client::send(const nlohmann::json &body) {
  co_await ws_->send(body.dump());
}

boost::asio::awaitable<::boost::beast::error_code>
ws_client::process_messages(const std::string &login_id,
                            const std::string &token) {
  while (!shutdown_) {
    auto [ec, buf] = co_await ws_->read_message();
    if (ec) {
      co_return ec;
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
  co_return boost::system::error_code();
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
  auth_promise_.set_value();
  co_return;
}

void ws_client::deliver_tick(tick &&t) {
  if (tick_callback_) {
    tick_callback_(t);
  }
}

void ws_client::process_price_data(const nlohmann::json &msg) {
  const auto &data = msg["d"];

  for (const auto &key: grouping_map) {
    if (auto it = data.find(key.first);
      it != data.end() && it->is_array() && !it->empty()) {
      try {
        auto prices = it->get<std::vector<std::string> >();
        for (const auto &price: prices) {
          deliver_tick(parse_tick(price, key.second));
        }
      } catch (const nlohmann::json::exception &e) {
        std::cerr << "Error parsing price data: " << e.what() << std::endl;
      }
    }
  }
}

void ws_client::process_subscribe_response(const nlohmann::json &msg) {
  auto d = msg["d"];
  assert(d["HasError"].get<bool>() == false);
  auto prices = d["Current"].get<std::vector<std::string> >();
  auto g = string_to_price_type(d["PriceGrouping"].get<std::string>());
  for (const auto &p: prices) {
    deliver_tick(parse_tick(p, g));
  }
}
