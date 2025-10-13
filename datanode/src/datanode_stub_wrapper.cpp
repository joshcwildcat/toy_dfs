#include "datanode_stub_wrapper.h"

// Concrete implementation that wraps gRPC DataNodeService::Stub (CLIENT)
DataNodeStubWrapper::DataNodeStubWrapper(const std::string& address,
                                         int32_t port)
    : address_(address), port_(port) {
  std::string node_address = address + ":" + std::to_string(port);
  auto channel =
      grpc::CreateChannel(node_address, grpc::InsecureChannelCredentials());
  stub_ = dfs::DataNodeService::NewStub(channel);
}

grpc::Status DataNodeStubWrapper::PutChunk(const std::string& chunk_id,
                                           const std::string& data) {
  grpc::ClientContext context;
  dfs::PutChunkResponse response;

  std::unique_ptr<grpc::ClientWriterInterface<dfs::PutChunkRequest>> writer(
      stub_->PutChunk(&context, &response));

  dfs::PutChunkRequest request;
  request.set_chunk_id(chunk_id);
  request.set_data(data);

  if (!writer->Write(request)) {
    writer->WritesDone();
    return writer->Finish();
  }

  writer->WritesDone();
  return writer->Finish();
}

grpc::Status DataNodeStubWrapper::GetChunk(const std::string& chunk_id,
                                           std::vector<char>* data) {
  grpc::ClientContext context;
  dfs::GetChunkRequest request;
  request.set_chunk_id(chunk_id);
  dfs::GetChunkResponse response;

  std::unique_ptr<grpc::ClientReaderInterface<dfs::GetChunkResponse>> reader(
      stub_->GetChunk(&context, request));

  std::vector<char> temp_data;
  bool success = false;

  while (reader->Read(&response)) {
    temp_data.insert(temp_data.end(), response.data().begin(),
                     response.data().end());
    success = true;
  }

  grpc::Status status = reader->Finish();
  if (status.ok() && success) {
    *data = std::move(temp_data);
    return grpc::Status::OK;
  }

  return status;
}

grpc::Status DataNodeStubWrapper::DeleteChunk(const std::string& chunk_id) {
  grpc::ClientContext context;
  dfs::DeleteChunkRequest request;
  request.set_chunk_id(chunk_id);
  dfs::DeleteChunkResponse response;

  return stub_->DeleteChunk(&context, request, &response);
}

// Concrete factory implementation
std::unique_ptr<IDataNodeStubWrapper>
DataNodeStubWrapperFactory::CreateClient(const std::string& address,
                                         int32_t port) {
  return std::make_unique<DataNodeStubWrapper>(address, port);
}
