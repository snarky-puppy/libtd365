/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#include "api_client.h"
#include "error.h"
#include "http_client.h"
#include "utils.h"
#include <boost/beast.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace beast = boost::beast;   // from <boost/beast.hpp>
namespace http = beast::http;     // from <boost/beast/http.hpp>
namespace net = boost::asio;      // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl; // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>
using json = nlohmann::json;

static constexpr auto MAX_DEPTH = 4;

namespace {
std::string extract_ots(std::string_view url) {
  constexpr std::string_view key = "ots=";
  auto pos = url.find(key);
  if (pos == std::string_view::npos) {
    spdlog::error("extract_ots: missing parameter in '{}'", url);
    throw_api_error(api_error::extract_ots, url);
  }
  pos += key.size();
  auto end = url.find('&', pos);
  auto len = (end == std::string_view::npos ? url.size() : end) - pos;
  return std::string{url.substr(pos, len)};
}

std::string extract_login_id(std::string_view body) {
  constexpr std::string_view key = R"(id="hfLoginID" value=")";
  auto pos = body.find(key);
  if (pos == std::string_view::npos) {
    throw_api_error(api_error::parse_login_id, body.substr(0, 32));
  }
  pos += key.size();
  auto end = body.find('"', pos);
  if (end == std::string_view::npos) {
    throw_api_error(api_error::parse_login_id, body.substr(pos, 32));
  }
  return std::string{body.substr(pos, end - pos)};
}

template <typename T> T extract_d(const json &j) {
  try {
    return j.at("d").template get<T>();
  } catch (const json::exception &e) {
    spdlog::error("extract_d: {}", e.what());
    throw_api_error(api_error::json_parse);
  }
}

template <typename T>
auto make_post(http_client *client, std::string_view path,
               nlohmann::json &&body) -> awaitable<T> {
  auto resp = co_await client->post(path, "application/json", body.dump());
  if (resp.result() != boost::beast::http::status::ok) {
    spdlog::error("unexpected response: from {}: {}", path,
                  static_cast<unsigned>(resp.result()));
    throw_api_error(api_error::http_post, "path={} result={} body={}", path,
                    static_cast<unsigned>(resp.result()),
                    resp.body().substr(0, 32));
  }
  auto j = json::parse(resp.body());
  co_return extract_d<T>(j);
}

void check_session_status(
    const boost::beast::http::message<
        false, boost::beast::http::basic_string_body<char>> &resp) {
  auto j = json::parse(resp.body());
  auto status = j["d"]["Status"].get<int>();
  if (status != 0) {
    // TODO: implement logout
    spdlog::error("non-zero status: {}", status);
    throw_api_error(api_error::session_status);
  }
}
} // namespace

api_client::api_client() {}

api_client::~api_client() = default;

awaitable<std::pair<std::string, std::string>>
api_client::open_client(std::string_view path, int depth) {
  if (depth > MAX_DEPTH) {
    spdlog::error("max depth reached: {}", depth);
    throw_api_error(api_error::max_depth, "path={} depth={}", path, depth);
  }
  auto [ec, response] = co_await client_->get(path);
  if (ec) {
  }
  if (response.result() == http::status::ok) {
    // extract the ots value here while we have the path
    // GET /Advanced.aspx?ots=WJFUMNFE
    // ots is the name of the cookie with the session token
    auto ots = extract_ots(path);
    auto login_id = extract_login_id(response.body());
    co_return std::make_pair(ots, login_id);
  }
  if (response.result() != http::status::found) {
    spdlog::error("unexpected response: from {}: {}", path,
                  static_cast<unsigned>(response.result()));
    throw_api_error(api_error::login, "path={} result={} body={}", path,
                    static_cast<unsigned>(response.result()),
                    response.body().substr(0, 32));
  }
  const auto location = response.at(http::field::location);
  co_return co_await open_client(std::move(location), depth + 1);
}

awaitable<api_client::auth_info> api_client::open(boost::urls::url url) {
  client_ = std::make_unique<http_client>(url);
  auto [ots, login_id] = co_await open_client(url);
  auto token = client_->jar().get(ots);

  const auto referer = std::format("{}://{}/Advanced.aspx?ots={}", url.scheme(),
                                   url.host(), ots);

  client_->set_default_headers({
      {"Origin", url.buffer()},
      {"Referer", referer},
  });
  co_return auth_info{token.value, login_id};
}

awaitable<void> api_client::close() {
  timer_.cancel();
  co_return;
}

void api_client::start_session_loop() {
  static const auto path = "/UTSAPI.asmx/UpdateClientSessionID";
  static const auto hdrs =
      headers({{"Content-Type", "application/json; charset=utf-8"},
               {"X-Requested-With", "XMLHttpRequest"}});

  net::co_spawn(
      ctx_.executor,
      [self = shared_from_this()]() mutable -> net::awaitable<void> {
        auto timeout = std::chrono::seconds(60);
        while (true) {
          try {
            if (auto resp = co_await self->client_->post(path, hdrs);
                resp.result() != http::status::ok) {
              // can happen when using mitmproxy
              timeout = std::chrono::seconds(0);
            } else {
              timeout = std::chrono::seconds(60);
              check_session_status(resp);
            }
          } catch (const boost::system::system_error &ec) {
            if (ec.code() == http::error::end_of_stream) {
              timeout = std::chrono::seconds(0);
            } else {
              spdlog::error("session_loop error: {}", ec.what());
              throw_api_error(api_error::http_post, "path={} ec={}", path,
                              ec.what());
            }
          } catch (...) {
            spdlog::error("session_loop: unknown error");
            throw std::current_exception();
          }

          self->timer_.expires_after(timeout);
          co_await self->timer_.async_wait(self->ctx_.cancelable());
        }
        spdlog::info("api_client::session_loop exiting");
        co_return;
      },
      boost::asio::detached);
}

awaitable<std::vector<market_group>> api_client::get_market_super_group() {
  json body = {};
  return make_post<std::vector<market_group>>(
      client_.get(), "/UTSAPI.asmx/GetMarketSuperGroup", std::move(body));
}

awaitable<std::vector<market_group>> api_client::get_market_group(int id) {
  json body = {{"superGroupId", id}};
  return make_post<std::vector<market_group>>(
      client_.get(), "/UTSAPI.asmx/GetMarketGroup", std::move(body));
}

awaitable<std::vector<market>> api_client::get_market_quote(int id) {
  json body = {
      {"groupID", id},      {"keyword", ""},   {"popular", false},
      {"portfolio", false}, {"search", false},
  };
  return make_post<std::vector<market>>(
      client_.get(), "/UTSAPI.asmx/GetMarketQuote", std::move(body));
}
