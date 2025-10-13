#pragma once
#include <grpcpp/grpcpp.h>

#include <string>

#include "datanode/datanode.grpc.pb.h"
#include "datanode_config.h"

using dfs::DataNodeService;
using dfs::DeleteChunkRequest;
using dfs::DeleteChunkResponse;
using dfs::GetChunkRequest;
using dfs::GetChunkResponse;
using dfs::PutChunkRequest;
using dfs::PutChunkResponse;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::Status;

class DataNodeServiceImpl final : public DataNodeService::Service {
public:
  // Production factory method with optional configuration
  static std::unique_ptr<DataNodeServiceImpl>
  Create(const DataNodeConfig& config = DataNodeConfig{}) {
    return std::make_unique<DataNodeServiceImpl>(config);
  }

  // Constructor with configuration
  explicit DataNodeServiceImpl(const DataNodeConfig& config);

  Status PutChunk(ServerContext* context, ServerReader<PutChunkRequest>* reader,
                  PutChunkResponse* response) override;

  Status GetChunk(ServerContext* context, const GetChunkRequest* request,
                  grpc::ServerWriter<GetChunkResponse>* writer) override;

  Status DeleteChunk(ServerContext* context, const DeleteChunkRequest* request,
                     DeleteChunkResponse* response) override;

private:
  DataNodeConfig config_;
};
