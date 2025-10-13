#include "datanode_service_impl.h"

#include <fstream>
#include <iostream>
#include <string>

#include "datanode_utils.h"

using dfs::DataNodeService;
using dfs::DeleteChunkRequest;
using dfs::DeleteChunkResponse;
using dfs::GetChunkRequest;
using dfs::GetChunkResponse;
using dfs::PutChunkRequest;
using dfs::PutChunkResponse;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::Status;

// Constructor with configuration
DataNodeServiceImpl::DataNodeServiceImpl(const DataNodeConfig& config)
    : config_(config) {
}

Status DataNodeServiceImpl::PutChunk(ServerContext* context,
                                     ServerReader<PutChunkRequest>* reader,
                                     PutChunkResponse* response) {
  PutChunkRequest req;
  std::string chunk_id;
  std::ofstream file;
  bool first = true;
  while (reader->Read(&req)) {
    if (first) {
      chunk_id = req.chunk_id();
      std::string file_path = get_chunk_path(chunk_id, config_);
      file.open(file_path, std::ios::binary);
      if (!file) {
        return Status(grpc::StatusCode::INTERNAL, "Failed to open file");
      }
      first = false;
    }
    file.write(req.data().data(), req.data().size());
  }
  file.close();
  response->set_success(true);
  // std::cout << "Stored chunk: " << chunk_id << std::endl;
  return Status::OK;
}

Status
DataNodeServiceImpl::GetChunk(ServerContext* context,
                              const GetChunkRequest* request,
                              grpc::ServerWriter<GetChunkResponse>* writer) {
  std::string file_path = get_chunk_path(request->chunk_id(), config_);
  std::ifstream file(file_path, std::ios::binary);
  if (!file) {
    return Status(grpc::StatusCode::NOT_FOUND, "Chunk not found");
  }
  char buffer[4096];
  GetChunkResponse resp;
  while (file.read(buffer, sizeof(buffer))) {
    resp.set_data(buffer, file.gcount());
    if (!writer->Write(resp)) {
      return Status(grpc::StatusCode::INTERNAL, "Write failed");
    }
  }
  if (file.gcount() > 0) {
    resp.set_data(buffer, file.gcount());
    writer->Write(resp);
  }
  return Status::OK;
}

Status DataNodeServiceImpl::DeleteChunk(ServerContext* context,
                                        const DeleteChunkRequest* request,
                                        DeleteChunkResponse* response) {
  std::string file_path = get_chunk_path(request->chunk_id(), config_);
  if (std::remove(file_path.c_str()) == 0) {
    response->set_success(true);
    response->set_error_message("");
    return Status::OK;
  } else {
    // File doesn't exist (ENOENT) is actually success for deletion
    if (errno == ENOENT) {
      response->set_success(true);
      response->set_error_message("");
      return Status::OK;
    } else {
      response->set_success(false);
      response->set_error_message(std::strerror(errno));
      return Status::OK;
    }
  }
}
