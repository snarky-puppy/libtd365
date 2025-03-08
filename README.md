# TD365 Trading API Library

A C++ library for interfacing with the TD365 trading platform API.

## Overview

This library provides a modern C++ interface to access TD365 trading platform features:

- Authentication via OAuth
- Real-time WebSocket data streaming
- Order management
- Position tracking
- Market data access

## Requirements

- C++23 compatible compiler
- Boost 1.83+
- OpenSSL
- nlohmann_json
- zstd

## Building

```bash
mkdir build && cd build
cmake ..
make
```

## Usage Example

```cpp
#include "td365.h"
#include <iostream>

int main() {
    td365 client;
    
    // Connect to the platform
    client.connect("username", "password", "account_id");
    
    // Subscribe to market updates
    client.subscribe_to_prices({"EUR/USD", "GBP/USD"});
    
    // Get account information
    auto account = client.get_account_info();
    std::cout << "Account balance: " << account.balance << std::endl;
    
    // Place an order
    client.place_order("EUR/USD", td365::ORDER_BUY, 1.0, 1000);
    
    return 0;
}
```

## API Documentation

See the header files in the `src` directory for detailed API documentation.

## License

MIT

## Disclaimer

This is an unofficial library and is not affiliated with TD365 or Trade Nation. Use at your own risk.