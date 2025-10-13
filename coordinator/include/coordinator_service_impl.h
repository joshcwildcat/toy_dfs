#pragma once
#include <grpcpp/grpcpp.h>

#include <atomic>
#include <memory>
#include <thread>

#include "coordinator/coordinator.grpc.pb.h"
#include "datanode/datanode.grpc.pb.h"
#include "datanode_stub_wrapper.h"
#include "metadata_store.h"

using dfs::CoordinatorService;
using dfs::DataNodeService;
using dfs::DeleteFileRequest;
using dfs::DeleteFileResponse;
using dfs::GetFileRequest;
using dfs::GetFileResponse;
using dfs::PutFileRequest;
using dfs::PutFileResponse;
using grpc::ServerContext;
using grpc::Status;

class CoordinatorServiceImpl final : public CoordinatorService::Service {
public:
  // Node registry for replication support
  struct DataNodeInfo {
    int32_t node_id;
    std::string address;  // Extracted from gRPC connection
    int32_t port;
  };

  MetadataStore store_;

  // Production factory method - creates concrete client factory
  static std::unique_ptr<CoordinatorServiceImpl> Create();

  // Constructor with dependency injection for testing
  explicit CoordinatorServiceImpl(
      std::unique_ptr<IDataNodeStubWrapperFactory> client_factory);

  // Test-friendly constructor - uses concrete factory
  CoordinatorServiceImpl();

  ~CoordinatorServiceImpl();  // Stop cleanup thread

  Status PutFile(ServerContext* context,
                 grpc::ServerReader<PutFileRequest>* reader,
                 PutFileResponse* response) override;

  Status GetFile(ServerContext* context, const GetFileRequest* request,
                 grpc::ServerWriter<GetFileResponse>* writer) override;

  Status DeleteFile(ServerContext* context, const DeleteFileRequest* req,
                    DeleteFileResponse* resp) override;

  // Replication support - DataNode registration
  Status RegisterDataNode(ServerContext* context,
                          const dfs::RegisterDataNodeRequest* req,
                          dfs::RegisterDataNodeResponse* resp) override;
  Status UnregisterDataNode(ServerContext* context,
                            const dfs::UnregisterDataNodeRequest* req,
                            dfs::UnregisterDataNodeResponse* resp) override;

private:
  Status readChunkWithFallback(const std::string& chunk_id,
                               std::vector<char>* chunk_data,
                               const std::vector<int32_t>& node_ids);
  std::vector<int32_t> selectNodesForReplication(int32_t replication_factor);
  std::vector<std::string>
  resolveNodeAddresses(const std::vector<int32_t>& node_ids);
  Status distributeChunkToNodes(const std::string& chunk_id,
                                const std::string& chunk_data,
                                const std::vector<int32_t>& node_ids);
  Status distributeChunkToSingleNode(const std::string& chunk_id,
                                     const std::string& chunk_data,
                                     int32_t node_id);

private:
  std::thread cleanup_thread_;
  std::atomic_flag running_{false};

  // Node registry for replication
  std::unordered_map<int32_t, DataNodeInfo> registered_nodes_;
  std::atomic<int32_t> next_node_id_{1};

  // Round-robin placement state for chunk distribution
  std::atomic<uint32_t> next_node_index_{0};

  // Factory for creating DataNode clients
  std::unique_ptr<IDataNodeStubWrapperFactory> client_factory_;

  void cleanupWorker();  // Background cleanup thread function

  // Helper function to extract client address from gRPC context
  std::string extractClientAddress(ServerContext* context);

public:
  // Test accessors for private members
  const std::unordered_map<int32_t, DataNodeInfo>& getRegisteredNodes() const {
    return registered_nodes_;
  }
  int32_t getNextNodeIndex() const { return next_node_index_.load(); }
};
