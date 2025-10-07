#include "dfs_client.h"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <put|get> <file_path>" << std::endl;
        return 1;
    }
    std::string command = argv[1];
    std::string file_path = argv[2];
    DFSClient client(grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials()));

    if (command == "put") {
        auto future = client.putFile(file_path);
        bool success = future.get();
        if (!success) {
            std::cerr << "Upload failed" << std::endl;
            return 1;
        }
    } else if (command == "get") {
        auto future = client.getFile(file_path);
        std::string content = future.get();
        std::cout.write(content.data(), content.size());
    } else {
        std::cout << "Unknown command: " << command << std::endl;
        return 1;
    }
    return 0;
}
