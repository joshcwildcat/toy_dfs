#include <grpcpp/grpcpp.h>

#include <iostream>

#include "datanode_service_impl.h"
#include "dfs_server.h"

void RunDataNodeServer() {
  std::string server_address("0.0.0.0:50052");
  auto service = DataNodeServiceImpl::Create();
  DFSServer<DataNodeServiceImpl> server(service.get(), server_address);
  server.Start();
  std::cout << "DataNode listening on " << server_address << std::endl;
  server.Wait();
}

int main() {
  RunDataNodeServer();
}
