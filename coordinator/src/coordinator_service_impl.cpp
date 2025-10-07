#include "coordinator_service_impl.h"

#include <grpcpp/grpcpp.h>

#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

using dfs::DeleteChunkRequest;
using dfs::DeleteChunkResponse;
using dfs::GetChunkRequest;
using dfs::GetChunkResponse;
using dfs::PutChunkRequest;
using dfs::PutChunkResponse;
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientWriter;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::Status;

// Factory method implementation
std::unique_ptr<CoordinatorServiceImpl>
CoordinatorServiceImpl::Create(const std::string& datanode_addr) {
  auto channel =
      grpc::CreateChannel(datanode_addr, grpc::InsecureChannelCredentials());
  auto stub = DataNodeService::NewStub(channel);
  return std::make_unique<CoordinatorServiceImpl>(std::move(stub));
}

// Constructor - starts background cleanup thread
CoordinatorServiceImpl::CoordinatorServiceImpl(
    std::unique_ptr<DataNodeService::StubInterface> stub)
    : datanode_stub_(std::move(stub)) {
  cleanup_thread_ = std::thread(&CoordinatorServiceImpl::cleanupWorker, this);
}

// Destructor - stops background cleanup thread
CoordinatorServiceImpl::~CoordinatorServiceImpl() {
  running_ = false;
  if (cleanup_thread_.joinable()) {
    cleanup_thread_.join();
  }
}

Status CoordinatorServiceImpl::PutFile(ServerContext* context,
                                       ServerReader<PutFileRequest>* reader,
                                       PutFileResponse* response) {
  PutFileRequest req;
  std::string filename;
  std::vector<char> file_data;

  bool first = true;
  while (reader->Read(&req)) {
    if (first) {
      filename = req.filename();
      first = false;
    }
    file_data.insert(file_data.end(), req.data().begin(), req.data().end());
  }
  if (filename.empty()) {
    return Status(grpc::StatusCode::INVALID_ARGUMENT, "Filename not provided");
  }

  // Chunk the data
  const int chunk_size = 1024 * 1024;  // 1 MB
  int num_chunks = (file_data.size() + chunk_size - 1) / chunk_size;
  FileMeta meta;
  meta.size = file_data.size();
  meta.chunk_size = chunk_size;

  static std::atomic<int> chunk_id_counter(0);
  for (int i = 0; i < num_chunks; ++i) {
    std::string chunk_id = "chunk_" + std::to_string(chunk_id_counter++);
    ChunkInfo c;
    c.chunk_id = chunk_id;
    c.locations = {"localhost:50052"};  // stub - hardcoded for now
    meta.chunks.push_back(c);

    // Send chunk to datanode
    ClientContext ctx;
    PutChunkResponse chunk_resp;
    std::unique_ptr<grpc::ClientWriterInterface<PutChunkRequest>> writer(
        datanode_stub_->PutChunk(&ctx, &chunk_resp));
    PutChunkRequest chunk_req;
    chunk_req.set_chunk_id(chunk_id);
    size_t start = i * chunk_size;
    size_t end = std::min(start + chunk_size, file_data.size());
    chunk_req.set_data(&file_data[start], end - start);
    // std::cerr << "Sending chunk";
    if (!writer->Write(chunk_req)) {
      writer->WritesDone();
      Status status = writer->Finish();
      std::cerr << "Failed to write chunk " << chunk_id
                << " to datanode: " << status.error_message()
                << " (code: " << status.error_code() << ")" << std::endl;
      return Status(grpc::StatusCode::INTERNAL,
                    "Failed to write chunk: " + status.error_message());
    }
    writer->WritesDone();
    // std::cerr << "Finished sending chunk";
    Status status = writer->Finish();
    if (!status.ok()) {
      std::cerr << "Chunk " << chunk_id
                << " write finished with error: " << status.error_message()
                << " (code: " << status.error_code() << ")" << std::endl;
      return status;
    }
  }

  store_.putFile(filename, meta);
  response->set_success(true);
  return Status::OK;
}

Status
CoordinatorServiceImpl::GetFile(ServerContext* context,
                                const GetFileRequest* request,
                                grpc::ServerWriter<GetFileResponse>* writer) {
  FileMeta* meta = store_.getFile(request->filename());
  if (!meta) {
    return Status(grpc::StatusCode::NOT_FOUND, "File not found");
  }

  for (const auto& chunk : meta->chunks) {
    ClientContext ctx;
    GetChunkRequest chunk_req;
    chunk_req.set_chunk_id(chunk.chunk_id);
    GetChunkResponse chunk_resp;
    std::unique_ptr<grpc::ClientReaderInterface<GetChunkResponse>> reader(
        datanode_stub_->GetChunk(&ctx, chunk_req));

    while (reader->Read(&chunk_resp)) {
      GetFileResponse resp;
      resp.set_data(chunk_resp.data());
      if (!writer->Write(resp)) {
        return Status(grpc::StatusCode::INTERNAL, "Failed to write response");
      }
    }
    Status status = reader->Finish();
    if (!status.ok()) {
      return status;
    }
  }
  return Status::OK;
}

Status CoordinatorServiceImpl::LookupFile(ServerContext* context,
                                          const LookupFileRequest* req,
                                          LookupFileResponse* resp) {
  FileMeta* meta = store_.getFile(req->path());
  if (!meta)
    return Status(grpc::StatusCode::NOT_FOUND, "File not found");
  for (const auto& c : meta->chunks) {
    resp->add_chunk_ids(c.chunk_id);
    resp->add_locations(c.locations[0]);  // simplified
  }
  return Status::OK;
}

Status CoordinatorServiceImpl::DeleteFile(ServerContext* context,
                                          const DeleteFileRequest* req,
                                          DeleteFileResponse* resp) {
  FileMeta* meta = store_.getFile(req->path());
  if (!meta) {
    return Status(grpc::StatusCode::NOT_FOUND, "File not found");
  }

  // Mark file as deleted (tombstone) - file appears deleted immediately
  // but chunks remain until cleanup (prevents race with ongoing reads)
  store_.deletePath(req->path());
  resp->set_success(true);

  // Chunks will be deleted by the tombstone cleanup mechanism
  // This prevents race conditions with ongoing reads
  return Status::OK;
}

// Background cleanup worker - runs in separate thread
void CoordinatorServiceImpl::cleanupWorker() {
  while (running_) {
    // Sleep for 1 minute between cleanup runs
    for (int i = 0; i < 60 && running_; ++i) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (!running_)
      break;

    // Perform tombstone cleanup
    store_.cleanupTombstones([this](const FileMeta& meta) {
      // Delete chunks for this tombstone
      for (const auto& chunk : meta.chunks) {
        DeleteChunkRequest del_req;
        del_req.set_chunk_id(chunk.chunk_id);
        DeleteChunkResponse del_resp;

        ClientContext ctx;
        Status status = datanode_stub_->DeleteChunk(&ctx, del_req, &del_resp);
        if (!status.ok()) {
          std::cerr << "gRPC error deleting chunk " << chunk.chunk_id
                    << " during tombstone cleanup: " << status.error_message()
                    << std::endl;
        } else if (!del_resp.success()) {
          std::cerr << "Failed to delete chunk " << chunk.chunk_id
                    << " during tombstone cleanup: " << del_resp.error_message()
                    << std::endl;
        }
      }
    });
  }
}
