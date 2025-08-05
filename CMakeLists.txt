cmake_minimum_required(VERSION 3.14)
project(ccxt_cpp)

include_directories(
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/vendor/websocketpp
    ${PROJECT_SOURCE_DIR}/vendor/json
)

set(CMAKE_CXX_STANDARD 20)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)

add_library(deribit
    src/deribit.cpp
)

target_include_directories(deribit PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/include)

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
