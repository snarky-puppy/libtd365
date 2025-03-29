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
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <string>
#include <vector>

class api_client {
public:
    struct login_response {
        std::string token;
        std::string login_id;
    };

    explicit api_client(const boost::asio::any_io_executor &executor,
                        std::string_view host);

    // login returns a token used to authenticate the websocket
    boost::asio::awaitable<login_response> login(std::string path);

    // I think that this is some kind of keep-alive as it doesn't return anythjing
    boost::asio::awaitable<void> update_session_token();

    boost::asio::awaitable<std::vector<market_group> > get_market_super_group();

    boost::asio::awaitable<std::vector<market_group> >
    get_market_group(unsigned int id);

    boost::asio::awaitable<std::vector<market> > get_market_quote(unsigned int id);

private:
    http_client client_;

    boost::asio::awaitable<std::pair<std::string, std::string> > connect(std::string path);
};

#endif // API_CLIENT_H
