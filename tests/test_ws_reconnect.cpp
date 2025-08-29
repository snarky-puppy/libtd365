/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#include <td365/types.h>
#include <td365/ws_client.h>

#include <atomic>
#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/url.hpp>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <thread>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;
using nlohmann::json;

// Fake WebSocket server for testing
class fake_ws_server {
  public:
    fake_ws_server(net::io_context &ioc, unsigned short port)
        : ioc_(ioc), acceptor_(ioc, tcp::endpoint(tcp::v4(), port)) {
        // Get the actual port number assigned by the system
        port_ = acceptor_.local_endpoint().port();
    }

    boost::asio::awaitable<void> run(std::atomic<bool> &shutdown) {
        while (!shutdown.load()) {
            try {
                tcp::socket socket =
                    co_await acceptor_.async_accept(boost::asio::use_awaitable);

                // Handle WebSocket connection
                co_await handle_websocket_connection(std::move(socket),
                                                     shutdown);
            } catch (const std::exception &e) {
                if (!shutdown.load()) {
                    // Only log if not shutting down
                    spdlog::debug("Server accept error: {}", e.what());
                }
                break;
            }
        }
    }

    void set_disconnect_after_connect(bool disconnect) {
        disconnect_after_connect_ = disconnect;
    }

    void set_disconnect_delay(std::chrono::milliseconds delay) {
        disconnect_delay_ = delay;
    }

    int get_connection_count() const { return connection_count_.load(); }

    unsigned short get_port() const { return port_; }

  private:
    boost::asio::awaitable<void>
    handle_websocket_connection(tcp::socket socket,
                                std::atomic<bool> &shutdown) {
        try {
            websocket::stream<tcp::socket> ws(std::move(socket));

            // Accept the WebSocket handshake
            co_await ws.async_accept(boost::asio::use_awaitable);

            connection_count_++;

            json j = {{"t", "connectResponse"}};
            co_await ws.async_write(
                net::buffer(j.dump().c_str(), j.dump().size() + 1),
                boost::asio::use_awaitable);

            if (disconnect_after_connect_) {
                // Wait for specified delay then disconnect
                net::steady_timer timer(ioc_);
                timer.expires_after(disconnect_delay_);
                co_await timer.async_wait(boost::asio::use_awaitable);

                // Close the connection
                co_await ws.async_close(websocket::close_code::normal,
                                        boost::asio::use_awaitable);
            } else {
                // Stay connected and handle messages
                while (!shutdown.load()) {
                    try {
                        beast::flat_buffer buffer;
                        co_await ws.async_read(buffer,
                                               boost::asio::use_awaitable);

                        // Echo back any received message
                        co_await ws.async_write(buffer.data(),
                                                boost::asio::use_awaitable);

                    } catch (const std::exception &) {
                        // Connection closed by client
                        break;
                    }
                }
            }
        } catch (const std::exception &e) {
            spdlog::debug("WebSocket connection error: {}", e.what());
        }
    }

    net::io_context &ioc_;
    tcp::acceptor acceptor_;
    unsigned short port_;
    std::atomic<bool> disconnect_after_connect_ = false;
    std::chrono::milliseconds disconnect_delay_ =
        std::chrono::milliseconds(1000);
    std::atomic<int> connection_count_ = 0;
};

TEST_CASE("WebSocket client reconnection test - non-existent server",
          "[websocket][reconnect][none]") {
    // Create client with callbacks
    td365::user_callbacks callbacks;
    std::atomic<bool> connection_failed = false;

    callbacks.tick_cb = [](td365::tick &&) {
        // Do nothing - just a placeholder
    };

    // Create io_context for the client
    net::io_context client_ioc;
    std::atomic<bool> shutdown = false;

    // Test connection with reconnection logic to a non-existent server
    std::atomic<bool> coroutine_done = false;

    // Create client on heap to ensure proper lifetime management
    auto client = std::make_unique<td365::ws_client>(callbacks);

    boost::asio::co_spawn(
        client_ioc,
        [&]() -> boost::asio::awaitable<void> {
            try {
                // Try to connect to a non-existent server - this should fail
                // and trigger reconnection
                boost::urls::url url("wss://localhost:9999");
                co_await client->run(url, "test_login", "test_token", shutdown);

            } catch (const std::exception &e) {
                connection_failed = true;
                // This is expected when max reconnection attempts are reached
            }
            coroutine_done = true;
            co_return;
        },
        boost::asio::detached);

    // Run client in separate thread
    std::thread client_thread([&client_ioc]() { client_ioc.run(); });

    // Wait for reconnection attempts to be exhausted (5 attempts with
    // exponential backoff) 1000ms + 2000ms + 3000ms + 4000ms = 10 seconds +
    // some buffer
    std::this_thread::sleep_for(std::chrono::seconds(12));

    // Stop everything
    shutdown = true;

    // Wait for coroutine to complete
    while (!coroutine_done) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Give some time for cleanup before stopping the context
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    client_ioc.stop();

    if (client_thread.joinable()) {
        client_thread.join();
    }

    // Now it's safe to destroy the client
    client.reset();

    // Verify that connection failed as expected and reconnection was attempted
    REQUIRE(connection_failed == true);
}

TEST_CASE("WebSocket client reconnection test - server disconnects",
          "[websocket][reconnect][fake]") {
    // Create server and client io_contexts
    net::io_context server_ioc;
    net::io_context client_ioc;

    // Create fake server that will disconnect after connection
    fake_ws_server server(server_ioc, 0); // Use port 0 to let system assign
    server.set_disconnect_after_connect(true);
    server.set_disconnect_delay(std::chrono::milliseconds(500));

    std::atomic<bool> server_shutdown = false;
    std::atomic<bool> client_shutdown = false;

    // Start server
    boost::asio::co_spawn(
        server_ioc,
        [&]() -> boost::asio::awaitable<void> {
            co_await server.run(server_shutdown);
        },
        boost::asio::detached);

    // Run server in separate thread
    std::thread server_thread([&server_ioc]() { server_ioc.run(); });

    // Give server time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Get the actual port the server is listening on
    unsigned short server_port = server.get_port();

    // Create client with callbacks
    td365::user_callbacks callbacks;
    // std::atomic<bool> connection_succeeded = false;

    // Create client on heap to ensure proper lifetime management
    auto client = std::make_unique<td365::ws_client>(callbacks);

    // Test connection with reconnection logic to fake server
    std::atomic<bool> client_coroutine_done = false;
    boost::asio::co_spawn(
        client_ioc,
        [&]() -> boost::asio::awaitable<void> {
            try {
                boost::urls::url url("ws://localhost:" +
                                     std::to_string(server_port));
                co_await client->run(url, "test_login", "test_token",
                                     client_shutdown);

            } catch (const std::exception &e) {
                // Expected when max reconnection attempts are reached
                spdlog::debug("Client reconnection failed: {}", e.what());
            }
            client_coroutine_done = true;
            co_return;
        },
        boost::asio::detached);

    // Run client in separate thread
    std::thread client_thread([&client_ioc]() { client_ioc.run(); });

    // Wait for multiple connection attempts
    // Server disconnects after 500ms, client should retry with exponential
    // backoff
    std::this_thread::sleep_for(std::chrono::seconds(8));

    // Stop everything
    client_shutdown = true;
    server_shutdown = true;

    // Wait for client coroutine to complete
    while (!client_coroutine_done) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Give some time for cleanup before stopping the contexts
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    client_ioc.stop();
    server_ioc.stop();

    if (client_thread.joinable()) {
        client_thread.join();
    }

    if (server_thread.joinable()) {
        server_thread.join();
    }

    // Now it's safe to destroy the client
    client.reset();

    // Verify that multiple connections were attempted
    // Server should have received multiple connection attempts due to
    // reconnection
    REQUIRE(server.get_connection_count() > 1);

    spdlog::info("Server received {} connection attempts",
                 server.get_connection_count());
}
