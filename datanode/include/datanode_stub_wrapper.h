#pragma once

#include <memory>
#include <string>
#include <vector>

#include "datanode/datanode.grpc.pb.h"
#include <grpcpp/grpcpp.h>

// Interface for DataNode operations - abstracts away gRPC details for better
// testability
class IDataNodeStubWrapper {
public:
  virtual ~IDataNodeStubWrapper() = default;

  // Put a chunk of data to this DataNode
  virtual grpc::Status PutChunk(const std::string& chunk_id,
                                const std::string& data) = 0;

  // Get a chunk of data from this DataNode
  virtual grpc::Status GetChunk(const std::string& chunk_id,
                                std::vector<char>* data) = 0;

  // Delete a chunk from this DataNode
  virtual grpc::Status DeleteChunk(const std::string& chunk_id) = 0;
};

// Concrete implementation that wraps gRPC DataNodeService::Stub (CLIENT)
class DataNodeStubWrapper : public IDataNodeStubWrapper {
public:
  explicit DataNodeStubWrapper(const std::string& address, int32_t port);
  ~DataNodeStubWrapper() override = default;

  grpc::Status PutChunk(const std::string& chunk_id,
                        const std::string& data) override;
  grpc::Status GetChunk(const std::string& chunk_id,
                        std::vector<char>* data) override;
  grpc::Status DeleteChunk(const std::string& chunk_id) override;

private:
  std::string address_;
  int32_t port_;
  std::unique_ptr<dfs::DataNodeService::Stub> stub_;
};

// Factory interface for creating DataNode clients
class IDataNodeStubWrapperFactory {
public:
  virtual ~IDataNodeStubWrapperFactory() = default;

  // Create a client for communicating with a specific DataNode
  virtual std::unique_ptr<IDataNodeStubWrapper>
  CreateClient(const std::string& address, int32_t port) = 0;
};

// Concrete factory that creates real DataNode clients using gRPC
class DataNodeStubWrapperFactory : public IDataNodeStubWrapperFactory {
public:
  std::unique_ptr<IDataNodeStubWrapper> CreateClient(const std::string& address,
                                                     int32_t port) override;
};
