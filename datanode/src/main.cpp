#include <grpcpp/grpcpp.h>

#include <iostream>

#include "coordinator/coordinator.grpc.pb.h"
#include "datanode_server.h"
#include "datanode_service_impl.h"
#include "dfs_server.h"

void RunDataNodeServer() {
  std::string server_address("0.0.0.0:50052");
  std::string coordinator_address("localhost:50053");

  auto service = DataNodeServiceImpl::Create();
  DataNodeServer server(service.get(), server_address, coordinator_address);

  std::cout << "DataNode listening on " << server_address << std::endl;
  server.Start();  // This will automatically register with Coordinator

  server.Wait();
}

int main() {
  RunDataNodeServer();
}
