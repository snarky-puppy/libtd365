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
#include <boost/asio/detached.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <iostream>
#include <nlohmann/json.hpp>
#include <ranges>

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
using tcp = net::ip::tcp;
using net::awaitable;
using net::use_awaitable;
namespace ssl = boost::asio::ssl;
using json = nlohmann::json;

enum class payload_type {
  heartbeat,
  connect_response,
  authentication_response,
  unknown,
  subscribe_response,
  price_data
};

// Using to_string from parsing.h

constexpr auto port = "443";

payload_type string_to_payload_type(std::string_view str) {
  static const std::unordered_map<std::string_view, payload_type> lookup = {
      {"heartbeat", payload_type::heartbeat},
      {"connectResponse", payload_type::connect_response},
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
    : executor_(executor), ws_(executor, ssl_ctx), shutdown_(shutdown),
      auth_future_(auth_promise_.get_future()),
      tick_callback_(std::move(tick_callback)),
      disconnect_future_(disconnect_promise_.get_future()) {}

ws_client::~ws_client() {}

void ws_client::start_loop(std::string login_id, std::string token) {
  net::co_spawn(
      executor_,
      [this, token, login_id]() -> net::awaitable<void> {
        co_await process_messages(login_id, token);
      },
      net::detached);

  auth_future_.wait();
}

boost::asio::awaitable<void> ws_client::close() {
  if (connected_) {
    boost::system::error_code ec;
    beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(1));
    co_await ws_.async_close(boost::beast::websocket::close_code::normal,
                             boost::asio::redirect_error(use_awaitable, ec));
    connected_ = false;
    disconnect_promise_.set_value();
  }
}

boost::asio::awaitable<void> ws_client::subscribe(int quote_id) {
  co_await send({{"quoteId", quote_id},
                 {"priceGrouping", "Sampled"},
                 {"action", "subscribe"}});
}

boost::asio::awaitable<void> ws_client::unsubscribe(int quote_id) {
  co_await send({{"quoteId", quote_id},
                 {"priceGrouping", "Sampled"},
                 {"action", "unsubscribe"}});
}

void ws_client::wait_for_disconnect() {
  if (connected_) {
    // Wait for the disconnect_future to be ready
    disconnect_future_.wait();
  }
}

boost::asio::awaitable<void> ws_client::connect(const std::string &host) {
  auto const ep = co_await td_resolve(executor_, host, port);

  // Set a timeout on the operation
  beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));

  // Make the connection on the IP address we get from a lookup
  co_await beast::get_lowest_layer(ws_).async_connect(ep, use_awaitable);

  // Set SNI Hostname (many hosts need this to handshake
  // successfully)
  if (!SSL_set_tlsext_host_name(ws_.next_layer().native_handle(),
                                host.c_str())) {
    throw beast::system_error(static_cast<int>(::ERR_get_error()),
                              net::error::get_ssl_category());
  }

  // Set a timeout on the operation
  beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));

  // Set a decorator to change the User-Agent of the handshake
  ws_.set_option(
      websocket::stream_base::decorator([](websocket::request_type &req) {
        req.set(http::field::user_agent, UserAgent);
        // req.set(http::field::origin, "https://demo.tradedirect365.com");
      }));

  // Perform the SSL handshake
  co_await ws_.next_layer().async_handshake(ssl::stream_base::client,
                                            use_awaitable);

  // Turn off the timeout on the tcp_stream, because
  // the websocket stream has its own timeout system.
  beast::get_lowest_layer(ws_).expires_never();

  // Set suggested timeout settings for the websocket
  ws_.set_option(
      websocket::stream_base::timeout::suggested(beast::role_type::client));

  // Perform the websocket handshake
  co_await ws_.async_handshake(host, "/", use_awaitable);

  // Mark as connected and reset disconnect promise
  connected_ = true;
  // Create a new promise in case this is a reconnect
  disconnect_promise_ = std::promise<void>();
  disconnect_future_ = disconnect_promise_.get_future();
}

boost::asio::awaitable<void> ws_client::send(const nlohmann::json &body) {
  auto [ec, bytes] = co_await ws_.async_write(
      net::buffer(body.dump()), boost::asio::as_tuple(use_awaitable));
  if (ec != std::errc{}) {
    throw boost::system::system_error(ec);
  }
}

boost::asio::awaitable<void>
ws_client::process_messages(const std::string &login_id,
                            const std::string &token) {
  while (!shutdown_) {
    beast::flat_buffer buffer;

    auto [ec, bytes] =
        co_await ws_.async_read(buffer, boost::asio::as_tuple(use_awaitable));
    if (ec != std::errc{}) {
      std::println(std::cerr, "ws_client: {} ({})", ec.message(), ec.what());

      // Mark as disconnected and notify any waiting threads
      connected_ = false;
      disconnect_promise_.set_value();

      throw boost::system::system_error(ec);
    }

    std::string buf(static_cast<const char *>(buffer.cdata().data()),
                    buffer.cdata().size());

    auto msg = nlohmann::json::parse(buf);

    switch (string_to_payload_type(msg["t"].get<std::string>())) {
    case payload_type::connect_response:
      co_await process_connect_response(msg, login_id, token);
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
    default:
      std::cerr << "Unhandled message" << msg.dump() << std::endl;
    }
  }
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
ws_client::process_connect_response(const nlohmann::json &msg,
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
  // FIXME: assumes everything is good here
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

  for (const auto &key : grouping_map) {
    if (auto it = data.find(key.first);
        it != data.end() && it->is_array() && !it->empty()) {
      try {
        auto prices = it->get<std::vector<std::string>>();
        for (const auto &price : prices) {
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
  auto prices = d["Current"].get<std::vector<std::string>>();
  auto g = string_to_price_type(d["PriceGrouping"].get<std::string>());
  for (const auto &p : prices) {
    deliver_tick(parse_tick(p, g));
  }
}
