# TD365 Trading API Library

A C++ library for interfacing with the TD365 trading platform API.

## Overview

This library provides a modern C++ interface to access TD365 trading platform features:

- Authentication via OAuth
- Real-time WebSocket data streaming
- ~~Order management~~ TODO
- ~~Position tracking~~ TODO
- Market data access

## TODO

- code cleanup
- package version
- Use namespace abbreviations as per https://www.boost.org/doc/libs/1_87_0/libs/beast/doc/html/beast/using_io.html
- order management
- position/account tracking

## Requirements

- C++23 compatible compiler
- Boost 1.83+
- OpenSSL
- nlohmann_json
- zlib

## Building

### Standard Build

```bash
mkdir build && cd build
cmake ..
make
```

### Debian Package

To build a Debian package:

```bash
# Install build dependencies
sudo apt update
sudo apt install debhelper-compat cmake libboost-all-dev libssl-dev nlohmann-json3-dev zlib1g-dev libspdlog-dev catch2

# Build the package
dpkg-buildpackage -us -uc
```

This creates two packages:
- `libtd365-1` - Runtime shared library
- `libtd365-dev` - Development headers and static library

Install with:
```bash
sudo dpkg -i ../libtd365-*.deb
```

## Usage Example

Please see the `examples` directory

## API Documentation

See the header files in the `src` directory for detailed API documentation.

## License

The Prosperity Public License 3.0.0

## Disclaimer

This is an unofficial library and is not affiliated with TD365 or Trade Nation. Use at your own risk.
