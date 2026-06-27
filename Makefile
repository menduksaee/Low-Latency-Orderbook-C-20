# Project-level Makefile for Order Book Application
# Usage:
#   make test           - Build and run all tests (equivalent to build_and_test.sh)
#   make app            - Build and run the OrderBook application
#   make performance    - Build and run performance tests with optimized flags
#   make clean          - Clean all build artifacts
#   make help           - Show this help message

# Variables
BUILD_DIR := build
SRC_DIR := src
PERF_FLAGS := -std=gnu++20 -O3 -DNDEBUG -march=native -flto=auto -fno-omit-frame-pointer -pipe -pthread
CXX := g++
NPROC := $(shell nproc)

# Default target
.PHONY: all
all: help

# Test target - equivalent to build_and_test.sh
.PHONY: test
test:
	@echo "=== Building and Testing Order Book ==="
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake ..
	@cd $(BUILD_DIR) && make -j$(NPROC) && \
		echo "Build successful!" && \
		echo "Running OrderBook tests..." && \
		./OrderbookTest || \
		(echo "Build or tests failed!" && exit 1)

# App target - build and run the OrderBook application
.PHONY: app
app:
	@echo "=== Building OrderBook Application ==="
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake .. -DCMAKE_BUILD_TYPE=Release
	@cd $(BUILD_DIR) && make -j$(NPROC) OrderBookApp && \
		echo "" && \
		echo "Build successful!" && \
		echo "Executable location: $(shell pwd)/$(BUILD_DIR)/OrderBookApp" && \
		echo "" && \
		echo "=== Running OrderBook Application ===" && \
		./OrderBookApp; \
		if [ $$? -eq 0 ]; then \
			echo "Application exited successfully."; \
		else \
			echo "Application failed with exit code $$?"; \
			exit 1; \
		fi

# Performance test target - direct compilation with optimized flags
.PHONY: performance
performance:
	@echo "=== Building Performance Test with Optimized Flags ==="
	@echo "Compiler: $(CXX)"
	@echo "Flags: $(PERF_FLAGS)"
	@echo "Command: $(CXX) $(PERF_FLAGS) $(SRC_DIR)/OrderBook.cpp $(SRC_DIR)/PerfomarmanceTesting.cpp -o perf"
	@echo ""
	@$(CXX) $(PERF_FLAGS) \
		-I$(SRC_DIR) \
		$(SRC_DIR)/OrderBook.cpp \
		$(SRC_DIR)/PerfomarmanceTesting.cpp \
		-o perf && \
		echo "" && \
		echo "Performance test build successful!" && \
		echo "Executable location: $(shell pwd)/perf" && \
		echo "" && \
		echo "=== Running Performance Test ===" && \
		./perf || \
		(echo "Performance test build failed!" && exit 1)

# Clean target
.PHONY: clean
clean:
	@echo "=== Cleaning Build Artifacts ==="
	@rm -rf $(BUILD_DIR)
	@rm -f perf
	@echo "Clean complete!"

# Help target
.PHONY: help
help:
	@echo "Order Book Project Makefile"
	@echo ""
	@echo "Available targets:"
	@echo "  test        - Build and run all tests (equivalent to build_and_test.sh)"
	@echo "  app         - Build and run the OrderBook application with optimized flags"
	@echo "  performance - Build and run performance tests with direct g++ compilation"
	@echo "  clean       - Clean all build artifacts"
	@echo "  help        - Show this help message"
	@echo ""
	@echo "Examples:"
	@echo "  make test"
	@echo "  make app"
	@echo "  make performance"
	@echo "  make clean"
	@echo ""
	@echo "Build configuration:"
	@echo "  Compiler: $(CXX)"
	@echo "  Parallel jobs: $(NPROC)"
	@echo "  Performance flags: $(PERF_FLAGS)"

# Additional utility targets
.PHONY: cmake-configure
cmake-configure:
	@echo "=== Configuring CMake ==="
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake .. -DCMAKE_BUILD_TYPE=Release

.PHONY: cmake-build
cmake-build: cmake-configure
	@echo "=== Building with CMake ==="
	@cd $(BUILD_DIR) && make -j$(NPROC)

# Debug versions (without optimization)
.PHONY: app-debug
app-debug:
	@echo "=== Building OrderBook Application (Debug) ==="
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake .. -DCMAKE_BUILD_TYPE=Debug
	@cd $(BUILD_DIR) && make -j$(NPROC) OrderBookApp && \
		echo "" && \
		echo "Debug build successful!" && \
		echo "Executable location: $(shell pwd)/$(BUILD_DIR)/OrderBookApp" && \
		echo "" && \
		echo "=== Running OrderBook Application (Debug) ===" && \
		./OrderBookApp; \
		if [ $$? -eq 0 ]; then \
			echo "Application exited successfully."; \
		else \
			echo "Debug application failed with exit code $$?"; \
			exit 1; \
		fi

# Show build info
.PHONY: info
info:
	@echo "=== Build Information ==="
	@echo "Project root: $(shell pwd)"
	@echo "Build directory: $(BUILD_DIR)"
	@echo "Source directory: $(SRC_DIR)"
	@echo "Compiler: $(CXX)"
	@echo "Parallel jobs: $(NPROC)"
	@echo "Performance flags: $(PERF_FLAGS)"
	@echo ""
	@echo "CMake version: $(shell cmake --version 2>/dev/null | head -1 || echo 'Not found')"
	@echo "GCC version: $(shell $(CXX) --version 2>/dev/null | head -1 || echo 'Not found')"
