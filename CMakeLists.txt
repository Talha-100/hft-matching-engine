cmake_minimum_required(VERSION 3.16)
project(hft-sim LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)

include_directories(include)

# Main executable
add_executable(
        hft-sim
        src/main.cpp
        src/OrderBook.cpp
        src/Order.cpp
        src/Trade.cpp
)

# Enable testing and fetch GoogleTest
enable_testing()

include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/heads/main.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# Disable GMock to avoid missing directory error
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# Test executable
add_executable(
        tests
        tests/order_book_tests.cpp
        src/OrderBook.cpp
        src/Order.cpp
        src/Trade.cpp
)

target_link_libraries(
        tests
        gtest_main
        gtest
)

include(GoogleTest)
gtest_discover_tests(tests)