#!/bin/bash

# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake ..

# Build the project
make -j$(nproc)

# Check if build was successful
if [ $? -eq 0 ]; then
    echo "Build successful!"
    
    # Run the tests
    echo "Running OrderBook tests..."
    ./OrderbookTest
    
    # Run with verbose output to see individual test results
    echo -e "\nRunning tests with verbose output..."
    ./OrderbookTest --gtest_verbose
else
    echo "Build failed!"
    exit 1
fi
