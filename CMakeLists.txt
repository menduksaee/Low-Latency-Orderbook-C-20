cmake_minimum_required(VERSION 3.16)
project(OrderBook)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find GTest
find_package(GTest REQUIRED)

# Main library
add_library(OrderBookLib
    src/OrderBook.cpp
    src/include/OrderBook.hpp
    src/include/LevelInfo.hpp
    src/include/Order.hpp
    src/include/constants.hpp
    src/include/Usings.hpp
    src/include/OrderType.hpp
    src/include/TradeInfo.hpp
    src/include/ModifyOrder.hpp
    src/include/OrderSide.hpp
)

# Test executable
add_executable(OrderbookTest
    OrderbookTest/test.cpp
    OrderbookTest/pch.cpp
    OrderbookTest/pch.h
)

target_link_libraries(OrderbookTest
    OrderBookLib
    GTest::gtest
    GTest::gtest_main
)

# Copy test files to build directory
file(COPY OrderbookTest/TestFiles DESTINATION ${CMAKE_BINARY_DIR})

# Main application executable
add_executable(OrderBookApp
    src/OrderBookApp.cpp
)

target_link_libraries(OrderBookApp
    OrderBookLib
)

# Set specific compiler flags for OrderBookApp
target_compile_options(OrderBookApp PRIVATE
    -std=gnu++20
    -O3
    -DNDEBUG
    -march=native
    -flto=auto
    -fno-omit-frame-pointer
    -pipe
    -pthread
)

# Set linker flags for OrderBookApp
target_link_options(OrderBookApp PRIVATE
    -flto=auto
    -pthread
)

# Enable testing
enable_testing()
add_test(NAME OrderbookTests COMMAND OrderbookTest)

# Set working directory for tests
set_tests_properties(OrderbookTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
