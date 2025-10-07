#pragma once
#include <grpcpp/grpcpp.h>

#include <fstream>
#include <future>
#include <iostream>
#include <string>
#include <thread>

#include "coordinator/coordinator.grpc.pb.h"

using dfs::CoordinatorService;
using dfs::DeleteFileRequest;
using dfs::DeleteFileResponse;
using dfs::GetFileRequest;
using dfs::GetFileResponse;
using dfs::PutFileRequest;
using dfs::PutFileResponse;
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientWriter;
using grpc::Status;

class DFSClient {
public:
  DFSClient(std::shared_ptr<Channel> channel);

  // Constructor with address for testing
  DFSClient(const std::string& address);

  std::future<std::string> getFile(const std::string& filename);

  std::future<bool> putFile(const std::string& filename);

  std::future<bool> deleteFile(const std::string& filename);

private:
  std::unique_ptr<CoordinatorService::Stub> stub_;
  void putFileSync(const std::string& filename);
  std::string getFileSync(const std::string& filename);
  bool deleteFileSync(const std::string& filename);
};
