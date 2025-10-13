#pragma once

#include "datanode_config.h"
#include "datanode_service_impl.h"
#include "dfs_server.h"

/**
 * Specialized DataNode Server that automatically registers with Coordinator.
 * Extends the generic DFSServer with DataNode-specific initialization.
 */
class DataNodeServer : public DFSServer<DataNodeServiceImpl> {
public:
  /**
   * Constructor with configuration
   * @param service Pointer to the DataNode service implementation
   * @param address Server address (e.g., "0.0.0.0:50054")
   * @param coordinator_address Coordinator address (e.g., "localhost:50053")
   * @param config DataNode configuration for chunk storage paths
   */
  DataNodeServer(DataNodeServiceImpl* service, const std::string& address,
                 const std::string& coordinator_address = "localhost:50053",
                 const DataNodeConfig& config = DataNodeConfig{});

  /**
   * Start the server and register with Coordinator
   */
  void Start() override;

  /**
   * Stop the server and unregister with Coordinator
   */
  void Stop() override;

private:
  std::string coordinator_address_;
  int32_t listening_port_;
  int32_t node_id_{-1};  // Store assigned node ID for unregistration
  DataNodeConfig config_;

  /**
   * Register this DataNode with the Coordinator
   */
  bool registerWithCoordinator();

  /**
   * Unregister this DataNode with the Coordinator
   */
  bool unregisterWithCoordinator();
};
