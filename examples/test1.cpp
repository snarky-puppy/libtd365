#include <boost/asio.hpp>
#include <boost/url/url.hpp>
#include <boost/url/url_view.hpp>
#include <spdlog/spdlog.h>

int main() {
    auto s = std::string{"localhost:8080"};
    auto u = boost::urls::url_view(s);
    spdlog::info("{:p}", s.data());
    spdlog::info("{}", u.data());
    spdlog::info("{}", u.scheme());
    spdlog::info("{}", u.host());
}
