// tests/td_resolve_host_port_tests.cpp

#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <string_view>

// Forward declaration of the function under test.
// In practice, include the header where td_resolve_host_port is declared.
std::pair<std::string_view, std::string_view>
td_resolve_host_port(std::string_view host, std::string_view port);

static void clear_proxy_env() {
#if defined(_WIN32) || defined(_WIN64)
  _putenv_s("PROXY", "");
#else
  unsetenv("PROXY");
#endif
}

static void set_proxy_env(const char* value) {
#if defined(_WIN32) || defined(_WIN64)
  _putenv_s("PROXY", value);
#else
  setenv("PROXY", value, 1);
#endif
}

TEST_CASE("td_resolve_host_port respects absence of PROXY", "[resolve]") {
  clear_proxy_env();

  auto [resolved_host, resolved_port] = td_resolve_host_port("original.host", "5555");
  REQUIRE(resolved_host == "original.host");
  REQUIRE(resolved_port == "5555");
}

TEST_CASE("td_resolve_host_port parses PROXY when in 'host:port' form", "[resolve]") {
  clear_proxy_env();
  set_proxy_env("proxy.example.com:9090");

  auto [resolved_host, resolved_port] = td_resolve_host_port("ignored.host", "1234");
  REQUIRE(resolved_host == "proxy.example.com");
  REQUIRE(resolved_port == "9090");

  clear_proxy_env();
}

TEST_CASE("td_resolve_host_port uses default port when PROXY lacks ':'", "[resolve]") {
  clear_proxy_env();
  set_proxy_env("just-a-proxy");

  auto [resolved_host, resolved_port] = td_resolve_host_port("should.be.ignored", "4321");
  REQUIRE(resolved_host == "just-a-proxy");
  REQUIRE(resolved_port == "8080");

  clear_proxy_env();
}
