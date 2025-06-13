#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

struct object {
    std::string name;

    // Default constructor
    object(const std::string &n = "default") : name(n) {
        std::cout << "Constructor called: " << name << '\n';
    }

    // Destructor
    ~object() { std::cout << "Destructor called: " << name << '\n'; }

    // Copy constructor
    object(const object &other) : name(other.name) {
        std::cout << "Copy constructor called: " << name << '\n';
    }

    // Copy assignment
    object &operator=(const object &other) {
        if (this != &other) {
            name = other.name;
            std::cout << "Copy assignment called: " << name << '\n';
        }
        return *this;
    }

    // Move constructor
    object(object &&other) noexcept : name(std::move(other.name)) {
        std::cout << "Move constructor called: " << name << '\n';
    }

    // Move assignment
    object &operator=(object &&other) noexcept {
        if (this != &other) {
            name = std::move(other.name);
            std::cout << "Move assignment called: " << name << '\n';
        }
        return *this;
    }
};

using opt_t = std::optional<object>;

void test(opt_t t) {
    if (t.has_value()) {
        std::cout << "has value: " << t.value().name << '\n';
    } else {
        std::cout << "has no value\n";
    }
}

int main() {

    opt_t opt{object("hello")};
    std::cout << "=== post o ctor\n";

    test(std::move(opt));

    std::cout << "=== post test\n";

    return 0;
}
