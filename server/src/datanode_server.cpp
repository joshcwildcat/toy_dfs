#include "datanode_server.h"

#include <grpcpp/grpcpp.h>
#include <iostream>

#include "coordinator/coordinator.grpc.pb.h"
#include "datanode/datanode.grpc.pb.h"

// Constructor with configuration
DataNodeServer::DataNodeServer(DataNodeServiceImpl* service,
                               const std::string& address,
                               const std::string& coordinator_address,
                               const DataNodeConfig& config)
    : DFSServer<DataNodeServiceImpl>(service, address),
      coordinator_address_(coordinator_address),
      config_(config) {
  // Extract port from address (format: "host:port")
  size_t colon_pos = address.rfind(':');
  if (colon_pos != std::string::npos) {
    listening_port_ = std::stoi(address.substr(colon_pos + 1));
  } else {
    listening_port_ = 50052;  // Default fallback
  }
}

// Start the server and register with Coordinator
void DataNodeServer::Start() {
  // Start the gRPC server first
  DFSServer::Start();

  // Then register with Coordinator
  if (!registerWithCoordinator()) {
    std::cerr
        << "Warning: Failed to register with Coordinator, but continuing..."
        << std::endl;
  }
}

// Stop the server and unregister with Coordinator
void DataNodeServer::Stop() {
  // First unregister with Coordinator if we have a valid node ID
  if (node_id_ > 0) {
    if (!unregisterWithCoordinator()) {
      std::cerr
          << "Warning: Failed to unregister with Coordinator during shutdown"
          << std::endl;
    }
  }

  // Then stop the gRPC server
  DFSServer::Stop();
}

// Register this DataNode with the Coordinator
bool DataNodeServer::registerWithCoordinator() {
  try {
    // Create channel to Coordinator
    auto channel = grpc::CreateChannel(coordinator_address_,
                                       grpc::InsecureChannelCredentials());
    auto coordinator_stub = dfs::CoordinatorService::NewStub(channel);

    // Prepare registration request
    dfs::RegisterDataNodeRequest reg_request;
    reg_request.set_port(listening_port_);

    dfs::RegisterDataNodeResponse reg_response;
    grpc::ClientContext context;

    std::cout << "Registering DataNode with Coordinator at "
              << coordinator_address_ << " (listening on port "
              << listening_port_ << ")..." << std::endl;

    // Send registration request
    grpc::Status status = coordinator_stub->RegisterDataNode(
        &context, reg_request, &reg_response);

    if (status.ok() && reg_response.success()) {
      // Store the assigned node ID for later unregistration
      node_id_ = reg_response.node_id();
      std::cout
          << "Successfully registered with Coordinator. Assigned Node ID: "
          << node_id_ << std::endl;
      return true;
    } else {
      std::cerr << "Failed to register with Coordinator: "
                << (status.ok() ? reg_response.error_message()
                                : status.error_message())
                << std::endl;
      return false;
    }
  } catch (const std::exception& e) {
    std::cerr << "Exception during DataNode registration: " << e.what()
              << std::endl;
    return false;
  }
}

// Unregister this DataNode with the Coordinator
bool DataNodeServer::unregisterWithCoordinator() {
  if (node_id_ <= 0) {
    std::cerr << "Warning: Cannot unregister - invalid node ID: " << node_id_
              << std::endl;
    return false;
  }

  try {
    // Create channel to Coordinator
    auto channel = grpc::CreateChannel(coordinator_address_,
                                       grpc::InsecureChannelCredentials());
    auto coordinator_stub = dfs::CoordinatorService::NewStub(channel);

    // Prepare unregistration request
    dfs::UnregisterDataNodeRequest unreg_request;
    unreg_request.set_node_id(node_id_);

    dfs::UnregisterDataNodeResponse unreg_response;
    grpc::ClientContext context;

    std::cout << "Unregistering DataNode ID " << node_id_
              << " with Coordinator..." << std::endl;

    // Send unregistration request
    grpc::Status status = coordinator_stub->UnregisterDataNode(
        &context, unreg_request, &unreg_response);

    if (status.ok() && unreg_response.success()) {
      std::cout << "Successfully unregistered DataNode ID " << node_id_
                << std::endl;
      return true;
    } else {
      std::cerr << "Failed to unregister DataNode ID " << node_id_ << ": "
                << (status.ok() ? unreg_response.error_message()
                                : status.error_message())
                << std::endl;
      return false;
    }
  } catch (const std::exception& e) {
    std::cerr << "Exception during DataNode unregistration: " << e.what()
              << std::endl;
    return false;
  }
}
