#include "coordinator_service_impl.h"
#include "dfs_server.h"
#include <iostream>

void RunServer() {
    std::string server_address("0.0.0.0:50051");
    auto service = CoordinatorServiceImpl::Create();
    DFSServer<CoordinatorServiceImpl> server(service.get(), server_address);
    server.Start();
    std::cout << "Coordinator listening on " << server_address << std::endl;
    server.Wait();
}

int main() { RunServer(); }
