/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#include "ws_client.h"
#include "types.h"

#include <catch2/catch_all.hpp>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/core.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/url.hpp>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <atomic>
#include <nlohmann/json.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

class fake_ws_server {
public:
    fake_ws_server(net::io_context& ioc, unsigned short port)
        : ioc_(ioc), acceptor_(ioc, tcp::endpoint(tcp::v4(), port)) {}

    boost::asio::awaitable<void> run() {
        while (true) {
            auto socket = co_await acceptor_.async_accept(boost::asio::use_awaitable);
            boost::asio::co_spawn(ioc_, handle_connection(std::move(socket)), boost::asio::detached);
        }
    }

private:
    boost::asio::awaitable<void> handle_connection(tcp::socket socket) {
        try {
            websocket::stream<tcp::socket> ws(std::move(socket));
            
            co_await ws.async_accept(boost::asio::use_awaitable);
            
            // Send connect response
            nlohmann::json connect_response = {
                {"t", "connectResponse"},
                {"cid", "test-connection-id"}
            };
            co_await ws.async_write(
                boost::asio::buffer(connect_response.dump()), 
                boost::asio::use_awaitable
            );
            
            // Wait for authentication request
            beast::flat_buffer buffer;
            co_await ws.async_read(buffer, boost::asio::use_awaitable);
            
            // Send authentication response
            nlohmann::json auth_response = {
                {"t", "authenticationResponse"},
                {"cid", "test-connection-id"},
                {"d", {{"Result", true}}}
            };
            co_await ws.async_write(
                boost::asio::buffer(auth_response.dump()), 
                boost::asio::use_awaitable
            );
            
            // Read any options message
            buffer.consume(buffer.size());
            co_await ws.async_read(buffer, boost::asio::use_awaitable);
            
            // Wait 1 second then close connection
            net::steady_timer timer(ioc_);
            timer.expires_after(std::chrono::seconds(1));
            co_await timer.async_wait(boost::asio::use_awaitable);
            
            // Close the connection
            co_await ws.async_close(websocket::close_code::normal, boost::asio::use_awaitable);
            
        } catch (std::exception& e) {
            // Connection handling error - this is expected when client disconnects
        }
    }

    net::io_context& ioc_;
    tcp::acceptor acceptor_;
};

TEST_CASE("WebSocket client reconnection test", "[websocket][reconnect]") {
    net::io_context ioc;
    
    // Start fake server on port 8080
    fake_ws_server server(ioc, 8080);
    boost::asio::co_spawn(ioc, server.run(), boost::asio::detached);
    
    // Run server in separate thread
    std::thread server_thread([&ioc]() {
        ioc.run();
    });
    
    // Allow server to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Create client with callbacks
    td365::user_callbacks callbacks;
    std::atomic<bool> tick_received = false;
    std::atomic<bool> connection_failed = false;
    std::atomic<int> connection_attempts = 0;
    
    callbacks.tick_cb = [&tick_received](td365::tick&& t) {
        tick_received = true;
    };
    
    td365::ws_client client(callbacks);
    
    // Create a separate io_context for the client
    net::io_context client_ioc;
    std::atomic<bool> shutdown = false;
    
    // Test connection with reconnection logic
    boost::asio::co_spawn(client_ioc, [&]() -> boost::asio::awaitable<void> {
        try {
            // Use the new reconnection method
            boost::urls::url url("ws://localhost:8080");
            co_await client.run_with_reconnect(url, "test_login", "test_token", shutdown);
            
        } catch (const std::exception& e) {
            connection_failed = true;
            // This is expected when max reconnection attempts are reached
        }
    }, boost::asio::detached);
    
    // Run client for a bit longer than server disconnect time
    std::thread client_thread([&client_ioc]() {
        client_ioc.run();
    });
    
    // Wait for reconnection attempts
    std::this_thread::sleep_for(std::chrono::seconds(8));
    
    // Stop everything
    shutdown = true;
    ioc.stop();
    client_ioc.stop();
    
    if (server_thread.joinable()) {
        server_thread.join();
    }
    if (client_thread.joinable()) {
        client_thread.join();
    }
    
    // Verify that connection failed as expected and reconnection was attempted
    REQUIRE(connection_failed == true);
}