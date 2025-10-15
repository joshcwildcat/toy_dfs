#include <grpcpp/grpcpp.h>

#include <getopt.h>
#include <iostream>

#include "coordinator/coordinator.grpc.pb.h"
#include "datanode_server.h"
#include "datanode_service_impl.h"
#include "dfs_server.h"

std::string server_address("0.0.0.0:50052");
std::string coordinator_address("localhost:50051");

void print_usage(const char* program_name) {
  std::cout << "DataNode - Distributed File System DataNode Server"
            << std::endl;
  std::cout << std::endl;
  std::cout << "Usage: " << program_name << " [options]" << std::endl;
  std::cout << std::endl;
  std::cout << "Options:" << std::endl;
  std::cout << "  -l, --listen-address ADDR    Address to listen on (default: "
               "0.0.0.0:50052)"
            << std::endl;
  std::cout << "  -c, --coordinator-address ADDR  Coordinator address to "
               "connect to (default: localhost:50051)"
            << std::endl;
  std::cout << "  -h, --help                   Show this help message"
            << std::endl;
  std::cout << std::endl;
  std::cout << "Examples:" << std::endl;
  std::cout << "  " << program_name
            << "                           # Use default addresses"
            << std::endl;
  std::cout << "  " << program_name
            << " -l 0.0.0.0:50052           # Specify listen address only"
            << std::endl;
  std::cout << "  " << program_name
            << " -c localhost:50051         # Specify coordinator address only"
            << std::endl;
  std::cout << "  " << program_name
            << " -l 0.0.0.0:50052 -c localhost:50051  # Specify both addresses"
            << std::endl;
}

int parse_opts(int argc, char* argv[]) {
  static struct option long_options[] = {
      {"listen-address", required_argument, 0, 'l'},
      {"coordinator-address", required_argument, 0, 'c'},
      {"help", no_argument, 0, 'h'},
      {0, 0, 0, 0}};

  int opt;
  int option_index = 0;

  while ((opt = getopt_long(argc, argv, "l:c:h", long_options,
                            &option_index)) != -1) {
    switch (opt) {
      case 'l':
        server_address = optarg;
        break;
      case 'c':
        coordinator_address = optarg;
        break;
      case 'h':
        print_usage(argv[0]);
        exit(0);
      case '?':
        // getopt_long already printed an error message
        print_usage(argv[0]);
        exit(1);
      default:
        print_usage(argv[0]);
        exit(1);
    }
  }

  // Check for non-option arguments (should not be any)
  if (optind < argc) {
    std::cerr << "Error: Unexpected non-option arguments: ";
    while (optind < argc) {
      std::cerr << argv[optind++] << " ";
    }
    std::cerr << std::endl;
    print_usage(argv[0]);
    exit(1);
  }
}

int main(int argc, char* argv[]) {
  // Updates server_address and/or coordinator_address if needed. Exits on
  // command line error.
  parse_opts(argc, argv);

  auto service = DataNodeServiceImpl::Create();
  DataNodeServer server(service.get(), server_address, coordinator_address);

  server.Start();
  std::cout << "DataNode listening on " << server_address << std::endl;
  std::cout << "DataNode connecting to coordinator at " << coordinator_address
            << std::endl;

  server.Wait();
}
