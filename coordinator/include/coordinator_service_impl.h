#pragma once
#include <grpcpp/grpcpp.h>

#include <atomic>
#include <memory>
#include <thread>

#include "coordinator/coordinator.grpc.pb.h"
#include "datanode/datanode.grpc.pb.h"
#include "metadata_store.h"

using dfs::CoordinatorService;
using dfs::DataNodeService;
using dfs::DeleteFileRequest;
using dfs::DeleteFileResponse;
using dfs::GetFileRequest;
using dfs::GetFileResponse;
using dfs::LookupFileRequest;
using dfs::LookupFileResponse;
using dfs::PutFileRequest;
using dfs::PutFileResponse;
using grpc::ServerContext;
using grpc::Status;

class CoordinatorServiceImpl final : public CoordinatorService::Service {
public:
  MetadataStore store_;
  std::unique_ptr<DataNodeService::StubInterface> datanode_stub_;

  // Production factory method - creates stub internally
  static std::unique_ptr<CoordinatorServiceImpl>
  Create(const std::string& datanode_addr = "localhost:50052");

  // Test-friendly constructor - takes stub directly (dependency injection)
  CoordinatorServiceImpl(std::unique_ptr<DataNodeService::StubInterface> stub);

  ~CoordinatorServiceImpl();  // Stop cleanup thread

  Status PutFile(ServerContext* context,
                 grpc::ServerReader<PutFileRequest>* reader,
                 PutFileResponse* response) override;

  Status GetFile(ServerContext* context, const GetFileRequest* request,
                 grpc::ServerWriter<GetFileResponse>* writer) override;

  Status LookupFile(ServerContext* context, const LookupFileRequest* req,
                    LookupFileResponse* resp) override;

  Status DeleteFile(ServerContext* context, const DeleteFileRequest* req,
                    DeleteFileResponse* resp) override;

private:
  std::thread cleanup_thread_;
  std::atomic<bool> running_{true};

  void cleanupWorker();  // Background cleanup thread function
};
