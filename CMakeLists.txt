cmake_minimum_required(VERSION 3.16)
project(hft-sim LANGUAGES CXX)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set CMake policies to suppress warnings
cmake_policy(SET CMP0144 NEW)  # Use upper-case package name variables
cmake_policy(SET CMP0167 NEW)  # Use modern FindBoost approach

# Platform-specific configurations
if(APPLE)
    # Handle both Intel and Apple Silicon Macs
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
        set(BOOST_ROOT "/opt/homebrew/opt/boost")
    else()
        set(BOOST_ROOT "/usr/local/opt/boost")
    endif()
elseif(WIN32)
    # Windows-specific Boost configuration
    set(Boost_USE_STATIC_LIBS ON)
    set(Boost_USE_MULTITHREADED ON)
    # User can override BOOST_ROOT via cmake -DBOOST_ROOT=path/to/boost
endif()

# Find Boost with required components
find_package(Boost REQUIRED COMPONENTS system)
if(NOT Boost_FOUND)
    message(FATAL_ERROR "Boost not found. Please install Boost or specify BOOST_ROOT.")
endif()

# Main executable
add_executable(
    hft-sim
#    src/main.cpp
    src/OrderBook.cpp
    src/Order.cpp
    src/Trade.cpp
    src/EngineServer.cpp
    src/engine_server_main.cpp
)

# Configure target properties
target_include_directories(
        hft-sim
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${Boost_INCLUDE_DIRS}
)

target_link_libraries(
        hft-sim
    PRIVATE
        Boost::system
)

# Platform-specific compile definitions
if(WIN32)
    target_compile_definitions(
            hft-sim
        PRIVATE
            _WIN32_WINNT=0x0A00  # Target Windows 10
            BOOST_ALL_NO_LIB     # Disable auto-linking
    )
endif()

# Testing configuration
enable_testing()

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/heads/main.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

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

target_include_directories(
        tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(
        tests
    PRIVATE
        gtest_main
        gtest
)

include(GoogleTest)
gtest_discover_tests(tests)

# Installation rules
install(
        TARGETS hft-sim
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Package configuration
include(CPack)