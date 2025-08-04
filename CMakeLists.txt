cmake_minimum_required(VERSION 3.14)
project(ccxt_cpp)

include_directories(
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/vendor/websocketpp
    ${PROJECT_SOURCE_DIR}/vendor/json
)

set(CMAKE_CXX_STANDARD 17)

# Fix websocketpp compatibility with modern Boost
add_definitions(-DBOOST_ASIO_SEPARATE_COMPILATION)
add_definitions(-DBOOST_ASIO_DYN_LINK)
add_definitions(-DBOOST_ALL_DYN_LINK)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)

# Original Deribit implementation
add_library(deribit
    src/deribit.cpp
)

target_include_directories(deribit PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/include)
target_link_libraries(deribit 
    PUBLIC 
    OpenSSL::SSL
    OpenSSL::Crypto
    Boost::system
    Threads::Threads
)

# Improved CCXT library with async architecture
add_library(ccxt_improved
    src/deribit_improved.cpp
)

target_include_directories(ccxt_improved PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ccxt_improved 
    PUBLIC 
    OpenSSL::SSL
    OpenSSL::Crypto
    Boost::system
    Threads::Threads
)

add_executable(main main.cpp)

target_link_libraries(
    main
    PRIVATE
    deribit
    OpenSSL::SSL
    OpenSSL::Crypto
    Boost::system
    Threads::Threads
)

# Test executable for architecture without websocketpp
add_executable(test_architecture
    test_main.cpp
)

target_include_directories(test_architecture PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Simple version of original main without websocketpp issues
add_executable(main_simple
    main_simple.cpp
)

target_include_directories(main_simple PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Example using improved implementation
add_executable(example_improved 
    examples/example_improved.cpp
)

target_link_libraries(
    example_improved
    PRIVATE
    ccxt_improved
    OpenSSL::SSL
    OpenSSL::Crypto
    Boost::system
    Threads::Threads
)
