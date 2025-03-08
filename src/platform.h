/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include "api_client.h"
#include "td365.h"
#include "ws_client.h"
#include <boost/asio.hpp>
#include <nlohmann/json_fwd.hpp>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

class ws_client;
class api_client;

class platform : public std::enable_shared_from_this<platform> {
public:
  explicit platform(std::function<void(const tick&)> tick_callback = nullptr);

  ~platform();

  void connect(const std::string &username, const std::string &password,
               const std::string &account_id);

  void shutdown();

  void subscribe(int quote_id);
  
  // Block until websocket disconnects
  void wait_for_disconnect();

  void unsubscribe(int quote_id);

  std::vector<market_group> get_market_super_group();

  std::vector<market_group> get_market_group(int id);

  std::vector<market> get_market_quote(int id);

private:
  template <typename Awaitable>
  auto run_awaitable(Awaitable awaitable) -> typename Awaitable::value_type;

  auto run_awaitable(boost::asio::awaitable<void>) -> void;

  boost::asio::io_context io_context_;
  boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
      work_guard_;
  std::thread io_thread_;

  // Callback to handle ticks received from WebSocket
  void on_tick_received(const tick& t);
  
  // Process ticks on a separate thread
  void process_ticks_thread();
  
  std::unique_ptr<api_client> api_client_;
  ws_client ws_client_;
  
  // Thread-safe queue for ticks
  std::queue<tick> tick_queue_;
  std::mutex tick_queue_mutex_;
  std::condition_variable tick_queue_cv_;
  
  // User callback for ticks
  std::function<void(const tick&)> user_tick_callback_;
  
  // Thread for processing ticks
  std::thread tick_processing_thread_;
  
  std::atomic<bool> shutdown_{false};
};

#endif // PLATFORM_H
