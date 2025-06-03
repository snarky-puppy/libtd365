/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#include "utils.h"
#include "base64.hpp"
#include "http_client.h"
#include "nlohmann/json.hpp"
#include <boost/asio/ssl.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/detail/base64.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/url/url.hpp>
#include <charconv>
#include <execution_ctx.h>
#include <fstream>
#include <ranges>
#include <regex>
#include <tuple>

namespace td365 {
using json = nlohmann::json;
namespace beast = boost::beast;

boost::asio::ssl::context &ssl_ctx() {
  static auto ctx = [&]() {
    auto rv =
        boost::asio::ssl::context(boost::asio::ssl::context::tlsv12_client);
    SSL_CTX_set_keylog_callback(
        rv.native_handle(), [](const SSL *, const char *line) {
          static const auto fpath = std::getenv("SSLKEYLOGFILE");
          if (!fpath) {
            return;
          }
          std::ofstream out(fpath, std::ios::app);
          out << line << std::endl;
        });
    rv.set_default_verify_paths();
    return rv;
  }();
  return ctx;
}

std::string now_utc() {
  boost::posix_time::ptime now =
      boost::posix_time::second_clock::universal_time();
  return boost::posix_time::to_iso_string(now);
}

void print_exception(const std::exception_ptr &eptr) {
  try {
    if (eptr)
      std::rethrow_exception(eptr);
  } catch (const std::exception &e) {
    std::cout << "Exception: " << e.what() << '\n';
  } catch (...) {
    std::cout << "Unknown exception\n";
  }
}

json extract_jwt_payload(const json &jwt) {
  if (!jwt.is_string()) {
    throw std::runtime_error(jwt.dump());
  }

  // https://en.cppreference.com/w/cpp/ranges
  auto jwts = to_string(jwt);
  const static std::regex re(R"(^[^\.]+\.([^\.]+).*$)");
  std::smatch m;
  assert(std::regex_match(jwts, m, re));
  auto payload = m[1].str();

  while ((payload.size() & 3) != 0) {
    payload.push_back('=');
  }

  const auto decoded = base64::decode_into<std::string>(payload);
  const auto rv = json::parse(decoded);
  return rv; // see NVRO
}

std::string get_http_body(http_client::response const &res) {
  auto body = beast::buffers_to_string(res.body().data());
  if (res[beast::http::field::content_encoding] == "gzip") {
    auto const src = boost::iostreams::array_source{body.data(), body.size()};
    auto is = boost::iostreams::filtering_istream{};
    auto os = std::stringstream{};
    is.push(boost::iostreams::gzip_decompressor{});
    is.push(src);
    boost::iostreams::copy(is, os);
    body = os.str();
  }
  return body;
}
} // namespace td365
