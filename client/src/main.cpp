#include <getopt.h>
#include <iostream>

#include "dfs_client.h"

void print_usage(const char* program_name) {
  std::cout << "DFS Client - Distributed File System Client" << std::endl;
  std::cout << std::endl;
  std::cout << "Usage: " << program_name
            << " [options] <put|get|delete> <file_path>" << std::endl;
  std::cout << std::endl;
  std::cout << "Commands:" << std::endl;
  std::cout << "  put <file_path>     Upload a file to DFS" << std::endl;
  std::cout << "  get <file_path>     Download a file from DFS" << std::endl;
  std::cout << "  delete <file_path>  Delete a file from DFS" << std::endl;
  std::cout << std::endl;
  std::cout << "Options:" << std::endl;
  std::cout << "  -c, --coordinator-address ADDR  Coordinator address "
               "(default: localhost:50051)"
            << std::endl;
  std::cout << "  -h, --help                      Show this help message"
            << std::endl;
  std::cout << std::endl;
  std::cout << "Examples:" << std::endl;
  std::cout << "  " << program_name
            << " put myfile.txt                    # Upload file (default "
               "coordinator)"
            << std::endl;
  std::cout << "  " << program_name
            << " get myfile.txt                    # Download file (default "
               "coordinator)"
            << std::endl;
  std::cout
      << "  " << program_name
      << " -c localhost:50051 put myfile.txt  # Upload with custom coordinator"
      << std::endl;
  std::cout
      << "  " << program_name
      << " --coordinator-address coordinator.example.com:50051 get myfile.txt"
      << std::endl;
}

int main(int argc, char* argv[]) {
  static struct option long_options[] = {
      {"coordinator-address", required_argument, 0, 'c'},
      {"help", no_argument, 0, 'h'},
      {0, 0, 0, 0}};

  std::string coordinator_address("localhost:50051");

  int opt;
  int option_index = 0;

  // Parse options only (stop at first non-option argument)
  while ((opt = getopt_long(argc, argv, "c:h", long_options, &option_index)) !=
         -1) {
    switch (opt) {
      case 'c':
        coordinator_address = optarg;
        break;
      case 'h':
        print_usage(argv[0]);
        return 0;
      case '?':
        // getopt_long already printed an error message
        print_usage(argv[0]);
        return 1;
      default:
        print_usage(argv[0]);
        return 1;
    }
  }

  // Check that we have the required positional arguments
  if (argc - optind < 2) {
    std::cout << "Usage: " << argv[0]
              << " [options] <put|get|delete> <file_path>" << std::endl;
    std::cout << "Use --help for more information." << std::endl;
    return 1;
  }

  std::string command = argv[optind];
  std::string file_path = argv[optind + 1];

  DFSClient client(grpc::CreateChannel(coordinator_address,
                                       grpc::InsecureChannelCredentials()));

  if (command == "put") {
    auto future = client.putFile(file_path);
    try {
      bool success = future.get();
      if (!success) {
        std::cerr << "Upload failed" << std::endl;
        return 1;
      }
    } catch (const std::system_error& e) {
      if (e.code().value() == EEXIST) {
        std::cerr << file_path << ": File exists" << std::endl;
        return 1;
      } else {
        throw;
      }
    }
    std::cout << "Uploaded " << file_path << std::endl;
  } else if (command == "get") {
    auto future = client.getFile(file_path);
    try {
      std::string content = future.get();
      std::cout.write(content.data(), content.size());
    } catch (const std::system_error& e) {
      if (e.code().value() == ENOENT) {
        std::cerr << file_path << ": No such file or directory" << std::endl;
        return 1;
      } else {
        throw;
      }
    }
  } else if (command == "delete") {
    auto future = client.deleteFile(file_path);
    try {
      auto result = future.get();
      if (result) {
        std::cout << file_path << " deleted successfully" << std::endl;
      } else {
        // Not expected. Should have thrown exceptino on failure.
        std::cerr << "Unexpected error when deleting " << file_path
                  << std::endl;
        return 1;
      }
    } catch (const std::system_error& e) {
      if (e.code().value() == ENOENT) {
        std::cerr << file_path << ": No such file or directory" << std::endl;
        return 1;
      } else {
        throw;
      }
    }
  } else {
    std::cout << "Unknown command: " << command << std::endl;
    return 1;
  }
  return 0;
}
