/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <cctype>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <spdlog/spdlog.h>
#include <sstream>
#include <td365/cookiejar.h>
#include <td365/http_client.h>
#include <td365/utils.h>

namespace {
std::string trim(const std::string &str) {
    const auto start = str.find_first_not_of(" \t");
    if (start == std::string::npos)
        return "";
    const auto end = str.find_last_not_of(" \t");
    return str.substr(start, end - start + 1);
}

// Convert a std::tm (in GMT) to time_t.
std::time_t convert_gmt(const std::tm &tm) {
#ifdef _WIN32
    return _mkgmtime(const_cast<std::tm *>(&tm));
#else
    return timegm(const_cast<std::tm *>(&tm));
#endif
}
} // anonymous namespace

namespace td365 {
cookiejar::cookiejar(std::string file_name) : path_(std::move(file_name)) {
    std::ifstream file(path_);
    if (file) {
        std::string line;
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            cookie c;
            std::time_t expiry_time_val = 0;
            if (!(iss >> c.name >> c.value >> expiry_time_val)) {
                // If expiry not found, treat as session cookie.
                c.expiry_time = std::chrono::system_clock::time_point();
            } else {
                c.expiry_time = (expiry_time_val == 0)
                                    ? std::chrono::system_clock::time_point()
                                    : std::chrono::system_clock::from_time_t(
                                          expiry_time_val);
            }
            cookies_[c.name] = c;
        }
    }
}

void cookiejar::save() const {
    std::ofstream file(path_, std::ios::trunc);
    for (const auto &[key, c] : cookies_) {
        std::time_t expiry_time_val =
            (c.expiry_time == std::chrono::system_clock::time_point())
                ? 0
                : std::chrono::system_clock::to_time_t(c.expiry_time);
        file << c.name << " " << c.value << " " << expiry_time_val << "\n";
    }
}

void cookiejar::update(const http_response &res) {
    for (const auto &h : res) {
        if (h.name() == boost::beast::http::field::set_cookie) {
            std::string header_value = h.value();
            std::istringstream cookie_stream(header_value);
            std::string token;
            // Extract cookie name and value (before first ';')
            if (!std::getline(cookie_stream, token, ';')) {
                std::cerr << "Malformed Set-Cookie header: " << header_value
                          << "\n";
                continue;
            }
            token = trim(token);
            auto equal_pos = token.find('=');
            if (equal_pos == std::string::npos) {
                std::cerr << "Malformed cookie pair in header: " << header_value
                          << "\n";
                continue;
            }
            cookie cookie_obj;
            cookie_obj.name = token.substr(0, equal_pos);
            cookie_obj.value = token.substr(equal_pos + 1);
            // Leave expiry_time as default for session cookies

            // Process additional attributes.
            while (std::getline(cookie_stream, token, ';')) {
                token = trim(token);
                if (token.empty())
                    continue;
                auto attr_pos = token.find('=');
                if (attr_pos == std::string::npos)
                    continue;
                std::string attr_name = token.substr(0, attr_pos);
                std::string attr_value = token.substr(attr_pos + 1);
                std::transform(attr_name.begin(), attr_name.end(),
                               attr_name.begin(),
                               [](unsigned char c) { return std::tolower(c); });
                if (attr_name == "max-age") {
                    try {
                        int max_age = std::stoi(attr_value);
                        cookie_obj.expiry_time =
                            std::chrono::system_clock::now() +
                            std::chrono::seconds(max_age);
                    } catch (...) {
                        std::cerr
                            << "Malformed Max-Age in header: " << header_value
                            << "\n";
                    }
                } else if (attr_name == "expires") {
                    std::tm tm = {};
                    auto parsed = false;

                    std::vector formats = {"%a, %d %b %Y %H:%M:%S GMT",
                                           "%a, %d-%b-%Y %H:%M:%S GMT"};

                    for (const auto &format : formats) {
                        std::istringstream date_stream(attr_value);
                        date_stream >> std::get_time(&tm, format);
                        if (!date_stream.fail()) {
                            parsed = true;
                            break;
                        }
                    }
                    if (!parsed) {
                        std::cerr << "Malformed Expires date in header: "
                                  << header_value << "\n";
                    } else {
                        std::time_t tt = convert_gmt(tm);
                        cookie_obj.expiry_time =
                            std::chrono::system_clock::from_time_t(tt);
                    }
                }
            }
            // Overwrite any existing cookie with the same name.
            cookies_[cookie_obj.name] = cookie_obj;
        }
    }
}

void cookiejar::apply(http_request &req) {
    auto now = std::chrono::system_clock::now();
    // Remove expired cookies (for cookies with a non-default expiry_time)
    for (auto it = cookies_.begin(); it != cookies_.end();) {
        if (it->second.expiry_time != std::chrono::system_clock::time_point{} &&
            now >= it->second.expiry_time) {
            it = cookies_.erase(it);
        } else {
            ++it;
        }
    }
    // Combine valid cookies into a single "Cookie" header.
    std::string cookie_str;
    for (const auto &pair : cookies_) {
        if (!cookie_str.empty())
            cookie_str += "; ";
        cookie_str += pair.second.name + "=" + pair.second.value;
    }
    req.erase(boost::beast::http::field::cookie);
    if (!cookie_str.empty())
        req.insert(boost::beast::http::field::cookie, cookie_str);
}

cookiejar::cookie cookiejar::get(const std::string &name) const {
    try {
        return cookies_.at(name);
    } catch (const std::out_of_range &) {
        spdlog::error("Cookie not found: {}", name);
    } catch (...) {
        spdlog::error("Unknown error");
    }
    return {};
}
} // namespace td365
