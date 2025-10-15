#include <getopt.h>
#include <iostream>

#include "coordinator_service_impl.h"
#include "dfs_server.h"

std::string server_address("0.0.0.0:50051");

void print_usage(const char* program_name) {
  std::cout << "Coordinator - Distributed File System Coordinator Server"
            << std::endl;
  std::cout << std::endl;
  std::cout << "Usage: " << program_name << " [options]" << std::endl;
  std::cout << std::endl;
  std::cout << "Options:" << std::endl;
  std::cout << "  -l, --listen-address ADDR    Address to listen on (default: "
               "0.0.0.0:50051)"
            << std::endl;
  std::cout << "  -h, --help                   Show this help message"
            << std::endl;
  std::cout << std::endl;
  std::cout << "Examples:" << std::endl;
  std::cout << "  " << program_name
            << "                           # Use default address" << std::endl;
  std::cout << "  " << program_name
            << " -l localhost:50051         # Specify listen address"
            << std::endl;
  std::cout << "  " << program_name
            << " --listen-address 0.0.0.0:50051  # Specify listen address"
            << std::endl;
}

int parse_opts(int argc, char* argv[]) {
  static struct option long_options[] = {
      {"listen-address", required_argument, 0, 'l'},
      {"help", no_argument, 0, 'h'},
      {0, 0, 0, 0}};

  int opt;
  int option_index = 0;

  while ((opt = getopt_long(argc, argv, "l:h", long_options, &option_index)) !=
         -1) {
    switch (opt) {
      case 'l':
        server_address = optarg;
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

  // Check for non-option arguments
  if (optind < argc) {
    std::cerr << "Error: Positional arguments are not supported." << std::endl;
    std::cerr
        << "Please use --listen-address or -l to specify the server address."
        << std::endl;
    std::cerr << std::endl;
    std::cerr << "Example:" << std::endl;
    std::cerr << "  Old: " << argv[0] << " localhost:50051" << std::endl;
    std::cerr << "  New: " << argv[0] << " -l localhost:50051" << std::endl;
    std::cerr << std::endl;
    print_usage(argv[0]);
    exit(1);
  }
}

int main(int argc, char* argv[]) {
  // Updates server_address if needed. Exits on command line error.
  parse_opts(argc, argv);

  auto service = CoordinatorServiceImpl::Create();
  DFSServer<CoordinatorServiceImpl> server(service.get(), server_address);
  server.Start();
  if (!server.IsRunning()) {
    std::cerr << "Fatal error starting server" << std::endl;
    return 1;
  }
  std::cout << "Coordinator listening on " << server_address << std::endl;
  server.Wait();
}
