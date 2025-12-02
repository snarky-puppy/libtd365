/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#include <boost/asio.hpp>
#include <td365/authenticator.h>
#include <td365/td365.h>
#include <td365/ws_client.h>

namespace td365 {

td365::td365() = default;

td365::~td365() = default;

void td365::connect() {
    auto auth_detail = authenticator::authenticate();
    auto auth_info = rest_client_.connect(auth_detail.platform_url);
    ws_client_.connect(auth_detail.sock_host, auth_info.login_id,
                       auth_info.token);
}

void td365::connect(const std::string &username, const std::string &password,
                    const std::string &account_id) {
    auto auth_detail =
        authenticator::authenticate(username, password, account_id);
    auto auth_info = rest_client_.connect(auth_detail.platform_url);
    ws_client_.connect(auth_detail.sock_host, auth_info.login_id,
                       auth_info.token);
}

void td365::subscribe(int quote_id) { ws_client_.subscribe(quote_id); }

void td365::unsubscribe(int quote_id) { ws_client_.unsubscribe(quote_id); }

event td365::wait(std::optional<std::chrono::milliseconds> timeout) {
    return ws_client_.read_and_process_message(timeout);
}

std::vector<market_group> td365::get_market_super_group() {
    return rest_client_.get_market_super_group();
}

std::vector<market_group> td365::get_market_group(int id) {
    return rest_client_.get_market_group(id);
}

std::vector<market> td365::get_market_quote(int id) {
    return rest_client_.get_market_quote(id);
}

market_details_response td365::get_market_details(int id) {
    return rest_client_.get_market_details(id);
}

trade_response td365::trade(const trade_request &&request) {
    rest_client_.get_market_details(request.market_id);
    rest_client_.sim_trade(request);
    return rest_client_.trade(std::move(request));
}

std::vector<candle> td365::backfill(int market_id, int quote_id, size_t sz,
                                    chart_duration dur) {
    return rest_client_.backfill(market_id, quote_id, sz, dur);
}
} // namespace td365
