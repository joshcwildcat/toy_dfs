#pragma once
#include <grpcpp/grpcpp.h>

#include <string>

#include "datanode/datanode.grpc.pb.h"

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

// Helper functions
void create_directories_recursively(const std::string& path);
std::string get_chunk_path(const std::string& chunk_id);

class DataNodeServiceImpl final : public DataNodeService::Service {
public:
  // Production factory method - creates stub internally
  static std::unique_ptr<DataNodeServiceImpl>
  Create(const std::string& datanode_addr = "localhost:50052") {
    return std::make_unique<DataNodeServiceImpl>();
  }

  Status PutChunk(ServerContext* context, ServerReader<PutChunkRequest>* reader,
                  PutChunkResponse* response) override;

  Status GetChunk(ServerContext* context, const GetChunkRequest* request,
                  grpc::ServerWriter<GetChunkResponse>* writer) override;

  Status DeleteChunk(ServerContext* context, const DeleteChunkRequest* request,
                     DeleteChunkResponse* response) override;
};
