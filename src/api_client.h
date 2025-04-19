/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#ifndef API_CLIENT_H
#define API_CLIENT_H

#include "http_client.h"
#include "td365.h"
#include <boost/asio/awaitable.hpp>
#include <string>
#include <vector>

class api_client {
public:
    struct auth_info {
        std::string token;
        std::string login_id;
    };

    enum session_token_response {
        RETRY,
        FAILURE,
        LOGOUT,
        OK
    };

    explicit api_client(td_context_view ctx);

    // `open` simulates opening the web client page. Returns a token used to authenticate the websocket
    boost::asio::awaitable<auth_info> open(std::string host, std::string path);

    boost::asio::awaitable<void> close();

    // I think that this is some kind of keep-alive as it doesn't return anything useful.
    void start_session_loop(std::atomic<bool> &shutdown);

    boost::asio::awaitable<std::vector<market_group> > get_market_super_group();

    boost::asio::awaitable<std::vector<market_group> >
    get_market_group(unsigned int id);

    boost::asio::awaitable<std::vector<market> > get_market_quote(unsigned int id);

private:
    td_context_view ctx_;
    std::unique_ptr<http_client> client_;
    boost::asio::steady_timer timer_;

    boost::asio::awaitable<std::pair<std::string, std::string> > post_login_flow(std::string path);
};

#endif // API_CLIENT_H
