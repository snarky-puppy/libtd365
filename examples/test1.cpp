#include <boost/asio.hpp>
#include <boost/url/url.hpp>
#include <boost/url/url_view.hpp>
#include <spdlog/spdlog.h>

int main() {
    auto s = std::string{"http://localhost:8080"};
    auto u = boost::urls::url_view(s);
    spdlog::info("{:p}", s.data());
    spdlog::info("{:p}", u.data());
    spdlog::info("{:p}", u.scheme().data());
}
