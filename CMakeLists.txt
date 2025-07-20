cmake_minimum_required(VERSION 3.16)
project(hft-sim LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)

include_directories(include)
add_executable(
        hft-sim
        src/main.cpp
        src/OrderBook.cpp
        src/Order.cpp
)