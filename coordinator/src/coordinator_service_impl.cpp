#include "coordinator_service_impl.h"

#include <grpcpp/grpcpp.h>

#include <atomic>
#include <future>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <vector>

using dfs::DeleteChunkRequest;
using dfs::DeleteChunkResponse;
using dfs::GetChunkRequest;
using dfs::GetChunkResponse;
using dfs::PutChunkRequest;
using dfs::PutChunkResponse;
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientWriter;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::Status;

// Factory method implementation - creates seride containing
// DataNodeClientFactory
std::unique_ptr<CoordinatorServiceImpl> CoordinatorServiceImpl::Create() {
  auto client_factory = std::make_unique<DataNodeStubWrapperFactory>();
  return std::make_unique<CoordinatorServiceImpl>(std::move(client_factory));
}

// Constructor with dependency injection for testing
CoordinatorServiceImpl::CoordinatorServiceImpl(
    std::unique_ptr<IDataNodeStubWrapperFactory> client_factory)
    : client_factory_(std::move(client_factory)) {
}

// Default constructor - uses concrete factory for backward compatibility
CoordinatorServiceImpl::CoordinatorServiceImpl()
    : CoordinatorServiceImpl(std::make_unique<DataNodeStubWrapperFactory>()) {
}

// Destructor - stops background cleanup thread
CoordinatorServiceImpl::~CoordinatorServiceImpl() {
  if (running_.test()) {
    running_.clear();
    if (cleanup_thread_.joinable()) {
      cleanup_thread_.join();
    }
  }
}

// Strongly consistent. Returns success only after all data is replicated and
// durable
Status CoordinatorServiceImpl::PutFile(ServerContext* context,
                                       ServerReader<PutFileRequest>* reader,
                                       PutFileResponse* response) {
  PutFileRequest req;
  std::string filename;
  std::vector<char> file_data;

  bool first = true;
  while (reader->Read(&req)) {
    if (first) {
      filename = req.filename();
      first = false;
    }
    file_data.insert(file_data.end(), req.data().begin(), req.data().end());
  }
  if (filename.empty()) {
    return Status(grpc::StatusCode::INVALID_ARGUMENT, "Filename not provided");
  }

  // Chunk the data
  const int chunk_size = 1024 * 1024;  // 1 MB
  int num_chunks = (file_data.size() + chunk_size - 1) / chunk_size;
  FileMeta meta;
  meta.size = file_data.size();
  meta.chunk_size = chunk_size;

  // We need globally unique chunk ids. Just use a counter for now.
  static std::atomic<u_int64_t> chunk_id_counter(0);
  // TODO: Distribute the chunks asynchrnously.
  for (int i = 0; i < num_chunks; ++i) {
    std::string chunk_id = "chunk_" + std::to_string(chunk_id_counter++);
    ChunkInfo c;
    c.chunk_id = chunk_id;

    // Use round-robin placement for replication
    c.node_ids = selectNodesForReplication(c.replication_factor);
    if (c.node_ids.empty()) {
      std::cerr << "No nodes available for chunk " << chunk_id << " replication"
                << std::endl;
      return Status(grpc::StatusCode::INTERNAL,
                    "No nodes available for replication");
    }

    c.replication_factor = 3;
    meta.chunks.push_back(c);

    // Prepare chunk data for distribution
    size_t start = i * chunk_size;
    size_t end = std::min(start + chunk_size, file_data.size());
    std::string chunk_data(file_data.begin() + start, file_data.begin() + end);

    // Distribute chunk to selected nodes for replication.
    Status distribution_status =
        distributeChunkToNodes(chunk_id, chunk_data, c.node_ids);
    if (!distribution_status.ok()) {
      std::cerr << "Failed to distribute chunk " << chunk_id
                << " to nodes: " << distribution_status.error_message()
                << std::endl;
      return distribution_status;
    }
  }

  store_.putFile(filename, meta);
  response->set_success(true);
  return Status::OK;
}

Status
CoordinatorServiceImpl::GetFile(ServerContext* context,
                                const GetFileRequest* request,
                                grpc::ServerWriter<GetFileResponse>* writer) {
  FileMeta* meta = store_.getFile(request->filename());
  if (!meta) {
    return Status(grpc::StatusCode::NOT_FOUND, "File not found");
  }

  for (const auto& chunk : meta->chunks) {
    // Use replica-aware read with fallback logic
    std::vector<char> chunk_data;
    Status read_status =
        readChunkWithFallback(chunk.chunk_id, &chunk_data, chunk.node_ids);

    if (!read_status.ok()) {
      std::cerr << "Failed to read chunk " << chunk.chunk_id
                << " from any replica: " << read_status.error_message()
                << std::endl;
      return Status(grpc::StatusCode::INTERNAL,
                    "Failed to read chunk " + chunk.chunk_id + ": " +
                        read_status.error_message());
    }

    // Send chunk data to client
    GetFileResponse resp;
    resp.set_data(std::string(chunk_data.begin(), chunk_data.end()));
    if (!writer->Write(resp)) {
      return Status(grpc::StatusCode::INTERNAL, "Failed to write response");
    }
  }
  return Status::OK;
}

Status CoordinatorServiceImpl::DeleteFile(ServerContext* context,
                                          const DeleteFileRequest* req,
                                          DeleteFileResponse* resp) {
  FileMeta* meta = store_.getFile(req->path());
  if (!meta) {
    return Status(grpc::StatusCode::NOT_FOUND, "File not found");
  }

  // Mark file as deleted (tombstone) - file appears deleted immediately
  // but chunks remain until cleanup (prevents race with ongoing reads)
  store_.deletePath(req->path());
  resp->set_success(true);

  // TODO: Move cleanup of deleted files elsewhere. We are creating the cleanup
  // thread lazilly here because joining the thread in the destructor takes too
  // long when running test_coordinator_service. This workaround works for now
  // since we aren't testing DeleteFile
  if (!running_.test()) {
    if (!running_.test_and_set()) {
      cleanup_thread_ =
          std::thread(&CoordinatorServiceImpl::cleanupWorker, this);
    }
  }

  // Chunks will be deleted by the tombstone cleanup mechanism
  // This prevents race conditions with ongoing reads
  return Status::OK;
}

// Replication support - DataNode registration
Status CoordinatorServiceImpl::RegisterDataNode(
    ServerContext* context, const dfs::RegisterDataNodeRequest* req,
    dfs::RegisterDataNodeResponse* resp) {
  try {
    // Extract actual client address from gRPC connection
    std::string client_address = extractClientAddress(context);

    // Check for existing address:port registration (business logic validation)
    for (const auto& [existing_id, existing_info] : registered_nodes_) {
      if (existing_info.address == client_address &&
          existing_info.port == req->port()) {
        // Conflict detected - return success gRPC status with failure response
        resp->set_success(false);
        resp->set_error_message("Address:port " + client_address + ":" +
                                std::to_string(req->port()) +
                                " already registered with node ID " +
                                std::to_string(existing_id));
        return Status::OK;  // gRPC success, but registration failed
      }
    }

    // Assign unique node ID
    int32_t assigned_id = next_node_id_.fetch_add(1);

    // Register the DataNode
    DataNodeInfo info;
    info.node_id = assigned_id;
    info.address = client_address;
    info.port = req->port();

    registered_nodes_[assigned_id] = info;

    std::cout << "Registered DataNode ID " << assigned_id << " at "
              << client_address << ":" << req->port() << std::endl;

    // Return success with assigned ID
    resp->set_success(true);
    resp->set_node_id(assigned_id);
    return Status::OK;

  } catch (const std::exception& e) {
    std::cerr << "Error registering DataNode: " << e.what() << std::endl;
    resp->set_success(false);
    resp->set_error_message("Registration failed: " + std::string(e.what()));
    return Status(grpc::StatusCode::INTERNAL, "Registration failed");
  }
}

// Replication support - DataNode unregistration
Status CoordinatorServiceImpl::UnregisterDataNode(
    ServerContext* context, const dfs::UnregisterDataNodeRequest* req,
    dfs::UnregisterDataNodeResponse* resp) {
  try {
    int32_t node_id = req->node_id();

    // Check if node exists in registry
    auto it = registered_nodes_.find(node_id);
    if (it == registered_nodes_.end()) {
      std::cerr << "Warning: Attempted to unregister non-existent DataNode ID "
                << node_id << std::endl;
      resp->set_success(false);
      resp->set_error_message("Node ID " + std::to_string(node_id) +
                              " not found in registry");
      return Status(grpc::StatusCode::NOT_FOUND, "Node not found");
    }

    // Remove node from registry
    registered_nodes_.erase(it);

    std::cout << "Successfully unregistered DataNode ID " << node_id
              << std::endl;

    // Return success
    resp->set_success(true);
    return Status::OK;

  } catch (const std::exception& e) {
    std::cerr << "Error unregistering DataNode: " << e.what() << std::endl;
    resp->set_success(false);
    resp->set_error_message("Unregistration failed: " + std::string(e.what()));
    return Status(grpc::StatusCode::INTERNAL, "Unregistration failed");
  }
}

// Helper function to extract client address from gRPC context
std::string
CoordinatorServiceImpl::extractClientAddress(ServerContext* context) {
  // Get the peer address from gRPC context
  // Format: "ipv4:127.0.0.1:54321" or "ipv6:[::1]:54321"
  auto peer_info = context->peer();

  // Extract just the address part (remove port)
  size_t colon_pos = peer_info.rfind(':');
  if (colon_pos != std::string::npos) {
    return peer_info.substr(0, colon_pos);
  }

  // Fallback to the full peer info if parsing fails
  return peer_info;
}

// Replication helper methods

// Select nodes for replication using round-robin algorithm
std::vector<int32_t>
CoordinatorServiceImpl::selectNodesForReplication(int32_t replication_factor) {
  std::vector<int32_t> selected_nodes;

  // Validate replication factor
  if (replication_factor <= 0) {
    std::cerr << "Error: Invalid replication factor: " << replication_factor
              << std::endl;
    return selected_nodes;
  }

  if (replication_factor > 10) {
    std::cerr << "Warning: High replication factor: " << replication_factor
              << " (maximum recommended: 10)" << std::endl;
  }

  if (registered_nodes_.empty()) {
    std::cerr << "Error: No registered nodes available for replication"
              << std::endl;
    return selected_nodes;
  }

  // Get all registered node IDs
  std::vector<int32_t> node_ids;
  for (const auto& pair : registered_nodes_) {
    node_ids.push_back(pair.first);
  }

  if (node_ids.empty()) {
    std::cerr << "Error: No node IDs found in registry" << std::endl;
    return selected_nodes;
  }

  int32_t num_nodes = static_cast<int32_t>(node_ids.size());

  // Select nodes using round-robin algorithm with node reuse allowed
  // This ensures we maintain the desired replication factor even when
  // there are fewer nodes available than the replication factor
  int32_t start_index = next_node_index_.fetch_add(1) % num_nodes;

  for (int32_t i = 0; i < replication_factor; ++i) {
    int32_t node_index = (start_index + i) % num_nodes;
    selected_nodes.push_back(node_ids[node_index]);
  }
  /*
    std::cout << "Selected " << selected_nodes.size() << " nodes for
    replication: "; for (int32_t node_id : selected_nodes) { std::cout <<
    node_id << " ";
    }
    std::cout << std::endl;
    */

  return selected_nodes;
}

// Convert node IDs to network addresses for client responses
std::vector<std::string> CoordinatorServiceImpl::resolveNodeAddresses(
    const std::vector<int32_t>& node_ids) {
  std::vector<std::string> addresses;

  for (int32_t node_id : node_ids) {
    auto it = registered_nodes_.find(node_id);
    if (it != registered_nodes_.end()) {
      addresses.push_back(it->second.address + ":" +
                          std::to_string(it->second.port));
    } else {
      std::cerr << "Warning: Node ID " << node_id << " not found in registry"
                << std::endl;
      addresses.push_back("unknown:" + std::to_string(node_id));
    }
  }

  return addresses;
}

// Helper function to distribute chunk to a single node
Status CoordinatorServiceImpl::distributeChunkToSingleNode(
    const std::string& chunk_id, const std::string& chunk_data,
    int32_t node_id) {
  auto it = registered_nodes_.find(node_id);
  if (it == registered_nodes_.end()) {
    std::cerr << "Warning: Node ID " << node_id << " not found in registry"
              << std::endl;
    return Status(grpc::StatusCode::NOT_FOUND, "Node ID " +
                                                   std::to_string(node_id) +
                                                   " not found in registry");
  }

  // Use factory to create client for this specific node
  std::string node_address =
      it->second.address + ":" + std::to_string(it->second.port);
  auto node_client =
      client_factory_->CreateClient(it->second.address, it->second.port);

  // Send chunk to this node using client interface
  grpc::Status client_status = node_client->PutChunk(chunk_id, chunk_data);
  if (!client_status.ok()) {
    std::cerr << "Failed to write chunk " << chunk_id << " to node " << node_id
              << " at " << node_address << ": " << client_status.error_message()
              << " (code: " << client_status.error_code() << ")" << std::endl;
    return client_status;
  }

  // std::cout << "Successfully distributed chunk " << chunk_id
  //           << " to node " << node_id << " at " << node_address << std::endl;
  return Status::OK;
}

// Distribute chunk to multiple nodes for replication (async version)
Status CoordinatorServiceImpl::distributeChunkToNodes(
    const std::string& chunk_id, const std::string& chunk_data,
    const std::vector<int32_t>& node_ids) {
  if (node_ids.empty()) {
    return Status(grpc::StatusCode::INTERNAL,
                  "No nodes specified for chunk distribution");
  }

  // Launch async distribution tasks for all nodes concurrently
  std::vector<std::future<Status>> futures;
  futures.reserve(node_ids.size());

  for (int32_t node_id : node_ids) {
    // Capture chunk_id, chunk_data, and node_id by value for async lambda
    auto future = std::async(
        std::launch::async, [this, chunk_id, chunk_data, node_id]() -> Status {
          return distributeChunkToSingleNode(chunk_id, chunk_data, node_id);
        });
    futures.push_back(std::move(future));
  }

  // Collect results and aggregate errors
  std::vector<std::string> errors;
  bool all_successful = true;

  for (size_t i = 0; i < futures.size(); ++i) {
    Status status =
        futures[i].get();  // Wait for each async operation to complete

    if (!status.ok()) {
      all_successful = false;
      int32_t node_id = node_ids[i];
      std::string error_msg =
          "Node " + std::to_string(node_id) + ": " + status.error_message();
      errors.push_back(error_msg);
    }
  }

  // Return appropriate status based on results
  if (!all_successful) {
    std::string combined_errors = "Failed to distribute chunk to nodes: ";
    for (size_t i = 0; i < errors.size(); ++i) {
      if (i > 0)
        combined_errors += "; ";
      combined_errors += errors[i];
    }
    return Status(grpc::StatusCode::INTERNAL, combined_errors);
  }

  return Status::OK;
}

// Read chunk with fallback logic for replica-aware operations
Status CoordinatorServiceImpl::readChunkWithFallback(
    const std::string& chunk_id, std::vector<char>* chunk_data,
    const std::vector<int32_t>& node_ids) {
  if (node_ids.empty()) {
    return Status(grpc::StatusCode::INTERNAL,
                  "No nodes specified for chunk read");
  }

  // Try each node in order until one succeeds
  for (int32_t node_id : node_ids) {
    auto it = registered_nodes_.find(node_id);
    if (it == registered_nodes_.end()) {
      std::cerr << "Warning: Node ID " << node_id
                << " not found in registry, skipping" << std::endl;
      continue;
    }

    // Use factory to create client for this specific node
    std::string node_address =
        it->second.address + ":" + std::to_string(it->second.port);
    auto node_client =
        client_factory_->CreateClient(it->second.address, it->second.port);

    // Try to read chunk from this node using client interface
    std::vector<char> temp_data;
    grpc::Status client_status = node_client->GetChunk(chunk_id, &temp_data);
    if (client_status.ok()) {
      // Success - copy data and return
      *chunk_data = std::move(temp_data);
      // std::cout << "Successfully read chunk " << chunk_id
      //         << " from node " << node_id << " at " << node_address <<
      //         std::endl;
      return Status::OK;
    } else {
      std::cerr << "Failed to read chunk " << chunk_id << " from node "
                << node_id << " at " << node_address << ": "
                << client_status.error_message()
                << " (code: " << client_status.error_code() << ")" << std::endl;
    }
  }

  // All nodes failed
  return Status(grpc::StatusCode::INTERNAL,
                "Failed to read chunk from any replica node");
}

// Background cleanup worker - runs in separate thread
void CoordinatorServiceImpl::cleanupWorker() {
  while (running_.test()) {
    // Sleep for 1 minute between cleanup runs
    for (int i = 0; i < 60 && running_.test(); ++i) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (!running_.test())
      break;

    // Perform tombstone cleanup
    store_.cleanupTombstones([this](const FileMeta& meta) {
      // Delete chunks for this tombstone from all registered nodes
      for (const auto& chunk : meta.chunks) {
        // Delete chunk from all nodes that have replicas
        for (int32_t node_id : chunk.node_ids) {
          auto it = registered_nodes_.find(node_id);
          if (it == registered_nodes_.end()) {
            std::cerr << "Warning: Node ID " << node_id
                      << " not found in registry during cleanup" << std::endl;
            continue;
          }

          // Use factory to create client for this specific node
          std::string node_address =
              it->second.address + ":" + std::to_string(it->second.port);
          auto node_client = client_factory_->CreateClient(it->second.address,
                                                           it->second.port);

          DeleteChunkRequest del_req;
          del_req.set_chunk_id(chunk.chunk_id);
          DeleteChunkResponse del_resp;

          grpc::Status delete_status = node_client->DeleteChunk(chunk.chunk_id);
          if (!delete_status.ok()) {
            std::cerr << "gRPC error deleting chunk " << chunk.chunk_id
                      << " from node " << node_id
                      << " during tombstone cleanup: "
                      << delete_status.error_message() << std::endl;
          } else if (!del_resp.success()) {
            std::cerr << "Failed to delete chunk " << chunk.chunk_id
                      << " from node " << node_id
                      << " during tombstone cleanup: "
                      << del_resp.error_message() << std::endl;
          } else {
            std::cout << "Successfully deleted chunk " << chunk.chunk_id
                      << " from node " << node_id << " during cleanup"
                      << std::endl;
          }
        }
      }
    });
  }
}
