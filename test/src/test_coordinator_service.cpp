#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <chrono>
#include <filesystem>
#include <grpcpp/grpcpp.h>
#include "coordinator_service_impl.h"
#include "coordinator/coordinator.grpc.pb.h"
#include "datanode/datanode.grpc.pb.h"

// Using declarations for protobuf types
using dfs::DeleteFileRequest;
using dfs::DeleteFileResponse;
using dfs::DataNodeService;

// Mock for DataNodeService::StubInterface
class MockDataNodeStub : public dfs::DataNodeService::StubInterface {
public:
    // Provide default implementations for all pure virtual methods
    grpc::ClientWriterInterface<dfs::PutChunkRequest>* PutChunkRaw(
        grpc::ClientContext*, dfs::PutChunkResponse*) override { return nullptr; }
    grpc::ClientAsyncWriterInterface<dfs::PutChunkRequest>* AsyncPutChunkRaw(
        grpc::ClientContext*, dfs::PutChunkResponse*, grpc::CompletionQueue*, void*) override { return nullptr; }
    grpc::ClientAsyncWriterInterface<dfs::PutChunkRequest>* PrepareAsyncPutChunkRaw(
        grpc::ClientContext*, dfs::PutChunkResponse*, grpc::CompletionQueue*) override { return nullptr; }
    grpc::ClientReaderInterface<dfs::GetChunkResponse>* GetChunkRaw(
        grpc::ClientContext*, const dfs::GetChunkRequest&) override { return nullptr; }
    grpc::ClientAsyncReaderInterface<dfs::GetChunkResponse>* AsyncGetChunkRaw(
        grpc::ClientContext*, const dfs::GetChunkRequest&, grpc::CompletionQueue*, void*) override { return nullptr; }
    grpc::ClientAsyncReaderInterface<dfs::GetChunkResponse>* PrepareAsyncGetChunkRaw(
        grpc::ClientContext*, const dfs::GetChunkRequest&, grpc::CompletionQueue*) override { return nullptr; }
    grpc::ClientAsyncResponseReaderInterface<dfs::DeleteChunkResponse>* AsyncDeleteChunkRaw(
        grpc::ClientContext*, const dfs::DeleteChunkRequest&, grpc::CompletionQueue*) override { return nullptr; }
    grpc::ClientAsyncResponseReaderInterface<dfs::DeleteChunkResponse>* PrepareAsyncDeleteChunkRaw(
        grpc::ClientContext*, const dfs::DeleteChunkRequest&, grpc::CompletionQueue*) override { return nullptr; }

    // Mock the DeleteChunk method that we actually use
    MOCK_METHOD(grpc::Status, DeleteChunk,
        (grpc::ClientContext*, const dfs::DeleteChunkRequest&, dfs::DeleteChunkResponse*));
};
//using dfs::CoordinatorService;

// Test fixture for CoordinatorServiceImpl
class CoordinatorServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create mock stub for testing
        mock_stub_ = std::make_unique<MockDataNodeStub>();
        service = std::make_unique<CoordinatorServiceImpl>(std::move(mock_stub_));
        // Clean up any existing test data
        service->store_.deletePath("test_file");
        service->store_.deletePath("non_existent_file");
    }

    void TearDown() override {
        // Clean up test data
        if (service) {
            service->store_.deletePath("test_file");
            service->store_.deletePath("non_existent_file");
        }
    }

    std::unique_ptr<MockDataNodeStub> mock_stub_;
    std::unique_ptr<CoordinatorServiceImpl> service;
};

// Test DeleteFile on non-existent file
TEST_F(CoordinatorServiceTest, DeleteFileNonExistent) {
    DeleteFileRequest request;
    request.set_path("non_existent_file");
    DeleteFileResponse response;

    grpc::ServerContext context;
    grpc::Status status = service->DeleteFile(&context, &request, &response);

    // Should fail with error code NOT_FOUND
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.error_code(), grpc::StatusCode::NOT_FOUND);
}

// Test DeleteFile metadata deletion timing
TEST_F(CoordinatorServiceTest, DeleteFileMetadataDeletedImmediately) {
    // Set up test metadata
    FileMeta meta;
    meta.size = 1024;
    meta.chunk_size = 512;
    meta.chunks = {{"chunk_timing_1", {"localhost:50052"}}};
    service->store_.putFile("timing_test_file", meta);

    // Call DeleteFile
    DeleteFileRequest request;
    request.set_path("timing_test_file");
    DeleteFileResponse response;

    grpc::ServerContext context;
    grpc::Status status = service->DeleteFile(&context, &request, &response);

    // Should return immediately
    EXPECT_TRUE(status.ok());
    EXPECT_TRUE(response.success());

    // Metadata should be deleted immediately (before chunk deletion completes)
    FileMeta* check_meta = service->store_.getFile("timing_test_file");
    EXPECT_EQ(check_meta, nullptr);

    // Clean up
    service->store_.deletePath("timing_test_file");
}

// Note: Full testing of CoordinatorServiceImpl requires mocking gRPC components
// which is complex. For now, we have basic structure.
// In a production setup, we'd use gMock to mock DataNodeService::Stub
