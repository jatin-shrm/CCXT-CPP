# CCXT C++

A C++ implementation of cryptocurrency exchange trading library, inspired by the popular CCXT library.

## Overview

This project provides a C++ interface for interacting with various cryptocurrency exchanges. It includes support for:

- REST API calls (not implimented right now)
- WebSocket connections
- Multiple exchange implementations (Binance, Deribit, etc.)
- JSON handling with nlohmann/json
- WebSocket communication with websocketpp

## Project Structure

```
ccxt/
├── CMakeLists.txt          # Main CMake configuration
├── main.cpp               # Main application entry point
├── src/
│   ├── base/
│   │   └── exchange.hpp   # Base exchange interface
│   ├── deribit.cpp        # Deribit exchange implementation
│   └── include/
│       ├── binance.hpp    # Binance exchange header
│       └── deribit.hpp    # Deribit exchange header
├── tests/                 # Test files
└── vendor/                # Third-party dependencies
    ├── json/              # nlohmann/json library
    └── websocketpp/       # WebSocket++ library
```

## Dependencies

- **CMake** (3.10 or higher)
- **C++17** compatible compiler
- **nlohmann/json** (included in vendor/)
- **websocketpp** (included in vendor/)
- **Boost.Asio** (for websocketpp)

## Building

```bash
# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build the project
make
```

## Usage

```cpp
#include "src/include/binance.hpp"
#include "src/include/deribit.hpp"

// Example usage will be added here
```

## Supported Exchanges

- [ ] Binance
- [ ] Deribit
- [ ] More exchanges coming soon...

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- Inspired by the [CCXT library](https://github.com/ccxt/ccxt)
- Uses [nlohmann/json](https://github.com/nlohmann/json) for JSON handling
- Uses [WebSocket++](https://github.com/zaphoyd/websocketpp) for WebSocket communication 