#pragma once

#include <gmock/gmock.h>
#include <memory>
#include <string>
#include <vector>

#include "datanode/datanode.grpc.pb.h"
#include "datanode_stub_wrapper.h"
#include <grpcpp/grpcpp.h>

// Mock implementation for testing
class MockDataNodeClient : public IDataNodeStubWrapper {
public:
  MOCK_METHOD(grpc::Status, PutChunk,
              (const std::string& chunk_id, const std::string& data),
              (override));
  MOCK_METHOD(grpc::Status, GetChunk,
              (const std::string& chunk_id, std::vector<char>* data),
              (override));
  MOCK_METHOD(grpc::Status, DeleteChunk, (const std::string& chunk_id),
              (override));
};

class MockDataNodeClientFactory : public IDataNodeStubWrapperFactory {
public:
  MOCK_METHOD(std::unique_ptr<IDataNodeStubWrapper>, CreateClient,
              (const std::string& address, int32_t port));
};
