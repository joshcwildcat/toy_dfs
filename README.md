# ToyDFS - Distributed File System

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)]()
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)]()
[![CMake](https://img.shields.io/badge/CMake-3.16+-blue.svg)]()

ToyDFS is a distributed file system implementation in C++17. I used Cline using the x-ai/grok-code-fast-1 model as a code assistant within VS Code. It was built for exploratory/demonstration purposes. It currently only supports uploading, downloading and deleting files. There is rudimentary support for data fault tolerance with replication. Metadata however is not yet fault-tolerant or even durable.

- [🚀 Quick Start](#-quick-start)
- [🏗️ Architecture](#-architecture)
- [🔧 Development](#-development)
- [📖 API Reference](#-api-reference)
- [🔍 Key Features](#-key-features)
- [🧪 Testing Strategy](#-testing-strategy)
- [🔨 Building from Source](#-building-from-source)

## 🏗️ Architecture

ToyDFS implements a simple distributed file system architecture, with no directory support. It has three main components:

### Coordinator Service
- **Metadata Management**: Tracks file-to-chunk mappings and chunk locations
- **Chunk Coordination**: Manages chunk placement and replication
- **Garbage Collection**: Background cleanup of deleted file chunks using tombstone mechanism
- **Fault Tolerance**: Data chunks are replicated to N datanodes. Failed chunk reads will attept other replicas.

### DataNode Service
- **Chunk Storage**: Stores file chunks as local files
- **Chunk Operations**: Provides read/write/delete operations for chunks
- **Local Persistence**: Uses filesystem for chunk storage with configurable paths
- **Auto-Registration**: Automatically registers with Coordinator during startup
- **Replication Support**: Handles multiple replicas of chunks for fault tolerance
- **No heartbeats or chunk rebalancing yet**

### DFS Client
- **File Operations**: Provides put/get/delete file operations
- **Asynchronous API**: Uses `std::future` for non-blocking operations)

## 🚀 Quick Start

### Prerequisites & Setup

**Core Requirements:**
- **C++17** compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- **CMake** 3.16 or higher
- **Protocol Buffers** compiler (`protoc`)
- **gRPC** C++ libraries

***Quick Install (Ubuntu/Debian):***
```bash
sudo apt-get install build-essential cmake libprotobuf-dev protobuf-compiler libgrpc++-dev
# Includes: C++ compiler (g++), CMake, Protocol Buffers, gRPC
```

***macOS:***
```bash
# First install Xcode command line tools (includes C++ compiler)
xcode-select --install

# Install build dependencies
brew install cmake autoconf libtool pkg-config openssl

# Install LLVM tools (required for code coverage with Clang)
brew install llvm
export PATH="/opt/homebrew/opt/llvm/bin:$PATH"

# Build GRPC and Protobuf
# NOTE: By default this installs in /usr/local Set CMAKE_INSTALL_PREFIX for another location
git clone --recurse-submodules -b v1.75.1 https://github.com/grpc/grpc.git
cd grpc
cmake -DCMAKE_BUILD_TYPE=Release
make
sudo make install
# Update environment
export PATH="/usr/local/bin:$PATH"
export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH"
```

### Build & Run

```bash
# Clone the repository
git clone <repository-url>
cd toydfs

# Create build directory and configure
mkdir build && cd build
cmake ..

# Build all components and run all tests
make check

# Start the system
./coordinator/coordinator    # Terminal 1
./datanode/datanode          # Terminal 2

# Use the client
./client/client put local_file.txt remote_file.txt

# Additional client operations
./client/client get remote_file.txt downloaded_file.txt
./client/client delete remote_file.txt
```


## 🔧 Development

### Available Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `ENABLE_TESTING` | `ON` | Enable test targets |
| `ENABLE_COVERAGE` | `OFF` | Enable code coverage reporting |
| `ENABLE_SANITIZERS` | `OFF` | Enable address/undefined sanitizers |
| `TEST_VERBOSE_OUTPUT` | `OFF` | Enable verbose test output |
| `CMAKE_BUILD_TYPE` | `Release` | Build type (Debug/Release/...) |

### Testing

```bash
# Run all tests
make check

# Run tests with verbose output
make check_verbose

# Generate coverage report (if enabled) - TODO: NOT WORKING correctly
make coverage
```

### Code Quality

The project uses strict compiler warnings to maintain code quality:

- **Valuable warnings as errors**: `-Wuninitialized`, `-Wreturn-type`, `-Wformat=2`, etc.
- **System header exclusions**: gRPC/protobuf headers excluded from warnings
- **Modern C++**: C++17 standard with extensions disabled

## 📖 API Reference

### DFS Client

```cpp
#include "dfs_client.h"

// Constructor
DFSClient client("coordinator:50053");

std::future<bool> upload = client.putFile("local.txt");
std::future<std::string> download = client.getFile("remote.txt");
std::future<bool> deletion = client.deleteFile("remote.txt");
```

## 🔍 Key Features

### Chunk-Based Storage
- Files automatically split into 1MB chunks
- Chunks distributed across DataNodes with replication factor = 3
- Metadata tracked by Coordinator

### Asynchronous Operations
- All client operations return `std::future`
- Non-blocking file transfers
- Concurrent operation support

### Fault Tolerance
- Tombstone-based deletion
- Background chunk cleanup
- Graceful error handling

### Data Replication
- **Auto-Registration**: DataNodes automatically register with Coordinator on startup
- **Node ID Assignment**: Coordinator assigns unique integer IDs to registered DataNodes
- **Replication Factor**: Configurable replication level (default: 3 replicas per chunk)
- **Round-Robin Placement**: Simple load distribution across registered DataNodes
- **Address Discovery**: Coordinator extracts DataNode addresses from gRPC connections

## 🧪 Testing Strategy

There are some unit tests, most are not implemented and skipped. There is a good set of system tests in test/src/system_test.cpp

### Running Tests

```bash
# Build and run all tests
make check

# Run specific test
./build/bin/system_test

# Run with coverage (requires LLVM tools on macOS) - TODO: Not working correctly
cmake -DENABLE_COVERAGE=ON ..
make coverage
```
