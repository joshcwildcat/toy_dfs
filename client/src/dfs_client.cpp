#include "dfs_client.h"

#include <fstream>
#include <future>
#include <iostream>
#include <thread>

// Method implementations for DFSClient

DFSClient::DFSClient(std::shared_ptr<grpc::Channel> channel)
    : stub_(dfs::CoordinatorService::NewStub(channel)) {
}

DFSClient::DFSClient(const std::string& address)
    : stub_(dfs::CoordinatorService::NewStub(
          grpc::CreateChannel(address, grpc::InsecureChannelCredentials()))) {
}

std::future<bool> DFSClient::putFile(const std::string& filename) {
  auto promise = std::make_shared<std::promise<bool>>();
  auto future = promise->get_future();

  // Run upload asynchronously
  std::thread([this, filename, promise]() {
    try {
      putFileSync(filename);
      promise->set_value(true);
    } catch (const std::exception& e) {
      promise->set_exception(std::current_exception());
    }
  }).detach();

  return future;
}

void DFSClient::putFileSync(const std::string& filename) {
  std::ifstream file(filename, std::ios::binary);
  if (!file) {
    throw std::runtime_error("Failed to open file: " + filename);
  }

  grpc::ClientContext ctx;
  dfs::PutFileResponse resp;
  std::unique_ptr<grpc::ClientWriter<dfs::PutFileRequest>> writer(
      stub_->PutFile(&ctx, &resp));

  // Send filename first
  dfs::PutFileRequest req;
  req.set_filename(filename);
  if (!writer->Write(req)) {
    throw std::runtime_error("Failed to send filename");
  }

  // Stream file data
  char buffer[4096];
  while (file.read(buffer, sizeof(buffer))) {
    req.set_data(buffer, file.gcount());
    if (!writer->Write(req)) {
      throw std::runtime_error("Failed to send data chunk");
    }
  }
  if (file.gcount() > 0) {
    req.set_data(buffer, file.gcount());
    if (!writer->Write(req)) {
      throw std::runtime_error("Failed to send final data chunk");
    }
  }

  writer->WritesDone();
  grpc::Status status = writer->Finish();
  if (status.ok() && resp.success()) {
    // std::cout << "File uploaded successfully: " << filename << std::endl;
  } else if (status.error_code() == grpc::StatusCode::ALREADY_EXISTS) {
    // Filesystem error: file exists
    throw std::system_error(EEXIST, std::system_category(), "File exists");
  } else {
    throw std::runtime_error(status.error_message());
  }
}

std::future<std::string> DFSClient::getFile(const std::string& filename) {
  auto promise = std::make_shared<std::promise<std::string>>();
  auto future = promise->get_future();

  // Run download asynchronously
  std::thread([this, filename, promise]() {
    try {
      std::string content = getFileSync(filename);
      promise->set_value(std::move(content));
    } catch (const std::exception& e) {
      promise->set_exception(std::current_exception());
    }
  }).detach();

  return future;
}

std::string DFSClient::getFileSync(const std::string& filename) {
  grpc::ClientContext ctx;
  dfs::GetFileRequest req;
  req.set_filename(filename);

  std::unique_ptr<grpc::ClientReader<dfs::GetFileResponse>> reader =
      stub_->GetFile(&ctx, req);

  std::string content;
  dfs::GetFileResponse resp;
  while (reader->Read(&resp)) {
    content.append(resp.data());
  }

  grpc::Status status = reader->Finish();
  if (status.ok()) {
    return content;
  } else if (status.error_code() == grpc::StatusCode::NOT_FOUND) {
    // Filesystem error: file not found
    throw std::system_error(ENOENT, std::system_category(),
                            "No such file or directory");
  } else {
    throw std::runtime_error(status.error_message());
  }
}

std::future<bool> DFSClient::deleteFile(const std::string& filename) {
  auto promise = std::make_shared<std::promise<bool>>();
  auto future = promise->get_future();

  // Run delete asynchronously
  std::thread([this, filename, promise]() {
    try {
      bool success = deleteFileSync(filename);
      promise->set_value(success);
    } catch (const std::exception& e) {
      promise->set_exception(std::current_exception());
    }
  }).detach();

  return future;
}

bool DFSClient::deleteFileSync(const std::string& filename) {
  grpc::ClientContext ctx;
  dfs::DeleteFileRequest req;
  req.set_path(filename);

  dfs::DeleteFileResponse resp;
  grpc::Status status = stub_->DeleteFile(&ctx, req, &resp);

  if (status.ok() && resp.success()) {
    return true;  // Success
  } else if (status.error_code() == grpc::StatusCode::NOT_FOUND) {
    // Filesystem error: file not found
    throw std::system_error(ENOENT, std::system_category(),
                            "No such file or directory");
  } else {
    // Protocol/network error: include StatusCode in message
    std::string error_msg =
        "gRPC error [" + std::to_string(static_cast<int>(status.error_code())) +
        "]: " + status.error_message();
    throw std::runtime_error(error_msg);
  }
}
