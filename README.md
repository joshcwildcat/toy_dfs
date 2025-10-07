# ToyDFS - Distributed File System

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)]()
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)]()
[![CMake](https://img.shields.io/badge/CMake-3.16+-blue.svg)]()

ToyDFS is a distributed file system implementation in modern C++17, demonstrating key concepts in distributed systems design including chunk-based storage, service coordination, and fault tolerance.

- [🚀 Quick Start](#-quick-start)
- [🏗️ Architecture](#-architecture)
- [📁 Project Structure](#-project-structure)
- [🔧 Development](#-development)
- [📖 API Reference](#-api-reference)
- [🔍 Key Features](#-key-features)
- [🧪 Testing Strategy](#-testing-strategy)
- [🔨 Building from Source](#-building-from-source)
- [📊 Performance Considerations](#-performance-considerations)
- [📝 License](#-license)
- [🙋 Support](#-support)

## 🏗️ Architecture

ToyDFS implements a classic distributed file system architecture with three main components:

### Coordinator Service
- **Metadata Management**: Tracks file-to-chunk mappings and chunk locations
- **Chunk Coordination**: Manages chunk placement and replication
- **Garbage Collection**: Background cleanup of deleted file chunks using tombstone mechanism
- **Fault Tolerance**: Handles datanode failures gracefully

### DataNode Service
- **Chunk Storage**: Stores file chunks as local files
- **Chunk Operations**: Provides read/write/delete operations for chunks
- **Local Persistence**: Uses filesystem for chunk storage with configurable paths

### DFS Client
- **File Operations**: Provides put/get/delete file operations
- **Asynchronous API**: Uses `std::future` for non-blocking operations
- **Chunking**: Automatically splits large files into chunks for distributed storage

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

# Build all components
make

# Start the system
./coordinator/coordinator    # Terminal 1
./datanode/datanode          # Terminal 2

# Use the client
./client/client put local_file.txt remote_file.txt

# Additional client operations
./client/client get remote_file.txt downloaded_file.txt
./client/client delete remote_file.txt
```

## 📁 Project Structure

```
toydfs/
├── client/                 # DFS client implementation
│   ├── include/dfs_client.h
│   └── src/
│       ├── dfs_client.cpp
│       └── main.cpp
├── coordinator/            # Coordinator service
│   ├── include/
│   │   ├── coordinator_service_impl.h
│   │   └── metadata_store.h
│   └── src/
│       ├── coordinator_service_impl.cpp
│       └── main.cpp
├── datanode/               # DataNode service
│   ├── include/
│   │   ├── datanode_service_impl.h
│   │   └── utils.h
│   └── src/
│       ├── datanode_service_impl.cpp
│       ├── utils.cpp
│       └── main.cpp
├── server/                 # Generic gRPC server template
│   ├── include/dfs_server.h
│   └── src/                # (header-only)
├── proto/                  # Protocol buffer definitions
│   ├── coordinator/
│   │   └── coordinator.proto
│   └── datanode/
│       └── datanode.proto
├── test/                   # Integration tests
│   └── src/
│       ├── test_*.cpp
└── build/                  # Build output (generated)
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

// Asynchronous operations
std::future<bool> upload = client.putFile("local.txt");
std::future<std::string> download = client.getFile("remote.txt");
std::future<bool> deletion = client.deleteFile("remote.txt");

// Synchronous operations
client.putFileSync("local.txt");
std::string data = client.getFileSync("remote.txt");
bool deleted = client.deleteFileSync("remote.txt");
```

### Service Implementation

```cpp
// Coordinator Service
class CoordinatorServiceImpl : public CoordinatorService::Service {
    Status PutFile(ServerContext* context,
                   ServerReader<PutFileRequest>* reader,
                   PutFileResponse* response) override;

    Status GetFile(ServerContext* context,
                   const GetFileRequest* request,
                   ServerWriter<GetFileResponse>* writer) override;
};

// DataNode Service
class DataNodeServiceImpl : public DataNodeService::Service {
    Status PutChunk(ServerContext* context,
                    ServerReader<PutChunkRequest>* reader,
                    PutChunkResponse* response) override;

    Status GetChunk(ServerContext* context,
                    const GetChunkRequest* request,
                    ServerWriter<GetChunkResponse>* writer) override;
};
```

## 🔍 Key Features

### Chunk-Based Storage
- Files automatically split into 1MB chunks
- Chunks distributed across DataNodes
- Metadata tracked by Coordinator

### Asynchronous Operations
- All client operations return `std::future`
- Non-blocking file transfers
- Concurrent operation support

### Fault Tolerance
- Tombstone-based deletion (prevents race conditions)
- Background chunk cleanup
- Graceful error handling

### Modern C++ Design
- RAII resource management
- Smart pointers for memory safety
- Template-based server abstraction
- Protocol buffers for serialization

## 🧪 Testing Strategy

The project includes comprehensive integration tests:

- **Unit Tests**: Individual component testing
- **Integration Tests**: Full system testing with multiple clients
- **Concurrent Operations**: Testing under load
- **Error Conditions**: Network failures, missing files, etc.

### Running Integration Tests

```bash
# Build and run all tests
make check

# Run specific test
./build/test/test_integration

# Run with coverage (requires LLVM tools on macOS) - TODO: Not working correctly
cmake -DENABLE_COVERAGE=ON ..
make coverage
```

## 📊 Performance Considerations

- **Chunk Size**: 1MB chunks balance memory usage and network efficiency
- **Async Operations**: Non-blocking I/O for better resource utilization
- **Connection Reuse**: gRPC channels reused for multiple operations
- **Background Cleanup**: Non-blocking garbage collection

## 📝 License

This project is provided as an educational example. See individual source files for any specific licensing terms.

## 🙋 Support

For questions or issues:
1. Check existing tests for usage examples
