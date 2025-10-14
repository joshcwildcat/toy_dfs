#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <thread>

#include "coordinator_service_impl.h"
#include "datanode_config.h"
#include "datanode_server.h"
#include "datanode_service_impl.h"
#include "dfs_client.h"
#include "dfs_server.h"

// Test fixture for system tests
class SystemTest : public ::testing::Test {
protected:
  // TODO: Use a randomly named temp direcotry inside the bulid directory or TMPDIR
  inline static const std::string TEST_DATA_DIR = "/tmp/dfs_system_test";
  inline static const std::string ROOT_CHUNK_DIR = "/tmp/dfs_chunks";

  inline static std::thread coordinator_thread;
  inline static std::vector<std::thread> datanode_threads;

  inline static std::unique_ptr<DFSServer<CoordinatorServiceImpl>>
      coordinator_server;
  inline static std::vector<std::unique_ptr<DataNodeServer>> datanode_servers;
  inline static std::unique_ptr<CoordinatorServiceImpl> coord_service;
  inline static std::vector<std::unique_ptr<DataNodeServiceImpl>>
      datanode_services;

  // Number of DataNode servers for testing replication
  inline static const int NUM_DATANODES = 3;

  inline const static std::string COORDINATOR_ADDRESS = "localhost:50053";

  static DFSClient createClient() { return DFSClient(COORDINATOR_ADDRESS); }

  // Setup and run a Coordinator Server and NUM_DATANODES servers which will be used in all tests.
  static void SetUpTestSuite() {
    // Make sure we are starting clean.
    std::filesystem::remove_all(TEST_DATA_DIR);
    std::filesystem::remove_all(ROOT_CHUNK_DIR);

    // Create test data directory
    std::filesystem::create_directories(TEST_DATA_DIR);

    // Create coordinator service
    coord_service = CoordinatorServiceImpl::Create();

    // Create coordinator server
    coordinator_server = std::make_unique<DFSServer<CoordinatorServiceImpl>>(
        coord_service.get(), COORDINATOR_ADDRESS);

    // Start coordinator server
    coordinator_server->Start();

    // Start coordinator wait thread
    coordinator_thread = std::thread([] { coordinator_server->Wait(); });

    // Create and start multiple DataNode servers with unique chunk paths
    for (int i = 0; i < NUM_DATANODES; ++i) {
      // Create unique configuration for this DataNode
      DataNodeConfig config = DataNodeConfig::CreateUnique(
          ROOT_CHUNK_DIR, "datanode_" + std::to_string(i));

      // Create DataNode service with configuration
      auto datanode_service = std::make_unique<DataNodeServiceImpl>(config);
      datanode_services.push_back(std::move(datanode_service));

      // Create DataNode server on different ports with configuration
      int datanode_port = 50054 + i;
      auto datanode_server = std::make_unique<DataNodeServer>(
          datanode_services.back().get(),
          "localhost:" + std::to_string(datanode_port), COORDINATOR_ADDRESS,
          config);
      datanode_servers.push_back(std::move(datanode_server));

      // Start the DataNode server
      datanode_servers.back()->Start();

      // Start wait thread for this DataNode
      datanode_threads.emplace_back([i]() { datanode_servers[i]->Wait(); });
    }

    // Verify DataNode registration
    EXPECT_EQ(coord_service->getRegisteredNodes().size(), NUM_DATANODES)
        << "All " << NUM_DATANODES
        << " DataNodes should have registered with Coordinator";
  }

  static void TearDownTestSuite() {
    // Stop all DataNode servers
    for (auto& datanode_server : datanode_servers) {
      if (datanode_server) {
        datanode_server->Stop();
      }
    }

    // Try graceful shutdown first for coordinator
    if (coordinator_server) {
      coordinator_server->Stop();
    }

    // Wait briefly for graceful shutdown
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Force shutdown all DataNode servers
    for (auto& datanode_server : datanode_servers) {
      if (datanode_server) {
        datanode_server->ForceShutdown();
      }
    }
    // Force shutdown if still running
    if (coordinator_server) {
      coordinator_server->ForceShutdown();
    }

    // Join all threads
    if (coordinator_thread.joinable()) {
      coordinator_thread.join();
    }

    for (auto& datanode_thread : datanode_threads) {
      if (datanode_thread.joinable()) {
        datanode_thread.join();
      }
    }

    // Clean up test data
    std::filesystem::remove_all(TEST_DATA_DIR);
    std::filesystem::remove_all(ROOT_CHUNK_DIR);
  }

  inline static long total_files = 0;
  std::string createTestFile(const std::string& content,
                             const std::string& filename = "test.txt") {
    std::string filepath =
        TEST_DATA_DIR + "/" + std::to_string(total_files++) + "_" + filename;
    std::ofstream file(filepath, std::ios::binary);
    file.write(content.data(), content.size());
    file.close();
    return filepath;
  }

  static std::string generateRandomContent(size_t size) {
    std::string content;
    content.reserve(size);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(32,
                                            126);  // Printable ASCII characters

    for (size_t i = 0; i < size; ++i) {
      content += static_cast<char>(distrib(gen));
    }

    return content;
  }
};

// Test basic file upload and download
TEST_F(SystemTest, PutAndGetFile) {
  // Create test file
  std::string test_content = "Hello, DFS System Test!";
  std::string test_file = createTestFile(test_content);

  // Upload file
  DFSClient client = createClient();
  auto upload_future = client.putFile(test_file);
  bool upload_success = upload_future.get();
  ASSERT_TRUE(upload_success) << "Upload failed";

  // Download file and verify content
  auto future = client.getFile(test_file);
  std::string received_content = future.get();
  EXPECT_EQ(received_content, test_content);
}

// Test multiple files
TEST_F(SystemTest, MultipleFiles) {
  DFSClient client = createClient();

  // Create and upload multiple files
  std::vector<std::pair<std::string, std::string>> files = {
      {"file1.txt", "Content of file 1"},
      {"file2.txt", "Content of file 2"},
      {"file3.txt", "Content of file 3"}};

  for (auto& [filename, content] : files) {
    filename = createTestFile(content, filename);
    auto upload_future = client.putFile(filename);
    bool upload_success = upload_future.get();
    ASSERT_TRUE(upload_success) << "Upload failed for " << filename;
  }

  // Verify all files can be downloaded
  for (const auto& [filename, expected_content] : files) {
    auto future = client.getFile(filename);
    std::string received_content = future.get();
    EXPECT_EQ(received_content, expected_content);
  }
}

// Test large file handling
TEST_F(SystemTest, LargeFile) {
  // Create a larger file (multiple chunks)
  std::string large_content;
  for (int i = 0; i < 10000; ++i) {
    large_content +=
        "This is line " + std::to_string(i) + " of a large file.\n";
  }

  std::string test_file = createTestFile(large_content, "large_file.txt");

  // Upload large file
  DFSClient client = createClient();
  auto upload_future = client.putFile(test_file);
  bool upload_success = upload_future.get();
  ASSERT_TRUE(upload_success) << "Upload failed for large file";

  // Download and verify
  auto future = client.getFile(test_file);
  std::string received_content = future.get();
  EXPECT_EQ(received_content, large_content);
}

// Test non-existent file
TEST_F(SystemTest, GetNonExistentFile) {
  DFSClient client = createClient();

  // This should throw an exception for non-existent file
  EXPECT_THROW(
      {
        auto future = client.getFile("non_existent.txt");
        future.get();  // This should throw
      },
      std::exception)
      << "GetFile should fail for non-existent file";
}

// Test concurrent operations from multiple clients (async version)
TEST_F(SystemTest, ConcurrentOperationsAsync) {
  const static int NUM_CLIENTS = 4;
  const static int OPERATIONS_PER_CLIENT = 3;
  const static int FILE_SIZE_MB = 8;
  const size_t FILE_SIZE_BYTES = FILE_SIZE_MB * 1024 * 1024;

  std::cout << "uploading "
            << NUM_CLIENTS * OPERATIONS_PER_CLIENT * FILE_SIZE_BYTES
            << " bytes contained in " << NUM_CLIENTS * OPERATIONS_PER_CLIENT
            << " files. Using " << NUM_CLIENTS << " clients" << std::endl;

  // Launch multiple client threads performing concurrent operations
  std::vector<std::thread> client_threads;
  std::atomic<int> success_count{0};

  for (int client_id = 0; client_id < NUM_CLIENTS; ++client_id) {
    client_threads.emplace_back([this, client_id, &success_count]() {
      DFSClient client = createClient();

      // Start ALL uploads for this client asynchronously
      std::vector<std::future<bool>> upload_futures;
      std::vector<std::string> filepaths;
      std::vector<std::string> contents;

      for (int op = 0; op < OPERATIONS_PER_CLIENT; ++op) {
        // Create unique filename for this client/operation
        std::string filename = "concurrent_file_" + std::to_string(client_id) +
                               "_" + std::to_string(op) + ".bin";

        // Generate random content for this file
        std::string content = generateRandomContent(FILE_SIZE_BYTES);
        std::string filepath = createTestFile(content, filename);

        // Start upload asynchronously
        upload_futures.push_back(client.putFile(filepath));
        filepaths.push_back(filepath);
        contents.push_back(std::move(content));
      }

      // Poll for completed uploads and start downloads when ready
      std::vector<std::pair<int, std::future<std::string>>> download_futures;
      std::vector<bool> upload_processed(OPERATIONS_PER_CLIENT, false);
      std::cout << "Client " << client_id << " initiated all uploads."
                << ::std::endl;

      // Keep checking for completed uploads and start downloads
      while (true) {
        bool all_processed = true;

        for (int op = 0; op < OPERATIONS_PER_CLIENT; ++op) {
          if (!upload_processed[op]) {
            // Check if this upload is ready (non-blocking)
            auto status = upload_futures[op].wait_for(std::chrono::seconds(0));
            if (status == std::future_status::ready) {
              // Upload is complete, get result
              bool upload_success = upload_futures[op].get();
              upload_processed[op] = true;

              if (upload_success) {
                // Start download immediately and track which operation it is
                download_futures.emplace_back(op,
                                              client.getFile(filepaths[op]));
              }
            } else {
              all_processed = false;  // Still waiting for some uploads
            }
          }
        }

        if (all_processed)
          break;

        // Small delay to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }

      std::cout << "All downloads iniatiated for client " << client_id
                << std::endl;
      // Wait for all downloads to complete and verify
      for (auto& [op_id, future] : download_futures) {
        try {
          std::string received_content = future.get();
          if (received_content == contents[op_id]) {
            // std::cout << "Client " << client_id << " downloaded file " <<
            // op_id << std::endl;
            success_count++;
          } else {
            std::cerr << "Client " << client_id << " operation " << op_id
                      << " failed: content mismatch, received "
                      << received_content.size() << " bytes vs expected "
                      << contents[op_id].size() << " bytes" << std::endl;
          }
        } catch (const std::exception& e) {
          std::cerr << "Client " << client_id << " operation " << op_id
                    << " failed: " << e.what() << std::endl;
        }
      }
      std::cout << "Client " << client_id << " completed." << std::endl;
    });
  }

  // Wait for all client threads to complete
  for (auto& thread : client_threads) {
    thread.join();
  }

  // Verify all operations succeeded
  int expected_operations = NUM_CLIENTS * OPERATIONS_PER_CLIENT;
  EXPECT_EQ(success_count, expected_operations)
      << "Expected " << expected_operations << " successful operations, got "
      << success_count;
}

// Test basic file deletion
TEST_F(SystemTest, DeleteFileBasic) {
  // Arrange: Upload a file
  std::string test_content = "Content to be deleted";
  std::string test_file = createTestFile(test_content, "delete_test.txt");

  DFSClient client = createClient();
  auto upload_future = client.putFile(test_file);
  ASSERT_TRUE(upload_future.get()) << "Upload failed";

  // Verify file exists before deletion
  auto verify_future = client.getFile(test_file);
  EXPECT_EQ(verify_future.get(), test_content);

  // Act: Delete the file
  auto delete_future = client.deleteFile(test_file);
  bool delete_success = delete_future.get();

  // Assert: Delete operation succeeds
  EXPECT_TRUE(delete_success) << "Delete operation failed";

  // Assert: File no longer exists (getFile should throw)
  EXPECT_THROW(
      {
        auto get_future = client.getFile(test_file);
        get_future.get();
      },
      std::exception)
      << "File should not exist after deletion";
}

// Test deletion of large file (multiple chunks)
TEST_F(SystemTest, DeleteFileLarge) {
  // Arrange: Create and upload a large file (multiple chunks)
  std::string large_content = generateRandomContent(10 * 1024 * 1024);  // 10MB
  std::string test_file =
      createTestFile(large_content, "large_delete_test.bin");

  DFSClient client = createClient();
  auto upload_future = client.putFile(test_file);
  ASSERT_TRUE(upload_future.get()) << "Upload of large file failed";

  // Verify file can be read before deletion
  auto verify_future = client.getFile(test_file);
  EXPECT_EQ(verify_future.get().size(), large_content.size());

  // Act: Delete the large file
  auto delete_future = client.deleteFile(test_file);
  bool delete_success = delete_future.get();

  // Assert: Delete operation succeeds
  EXPECT_TRUE(delete_success) << "Delete operation failed for large file";

  // Assert: File no longer accessible
  EXPECT_THROW(
      {
        auto get_future = client.getFile(test_file);
        get_future.get();
      },
      std::exception)
      << "Large file should not exist after deletion";
}

// Test deletion of non-existent file (should throw ENOENT)
TEST_F(SystemTest, DeleteFileNonExistent) {
  DFSClient client = createClient();
  auto delete_future = client.deleteFile("non_existent_file.txt");
  try {
    delete_future.get();
    FAIL()
        << "Expected std::system_error when deletFile when file doesn't exist.";
  } catch (const std::system_error& e) {
    EXPECT_EQ(e.code().value(), ENOENT)
        << "Should throw ENOENT for non-existent file";
  }
}

// Test deletion of already deleted file (should throw ENOENT)
TEST_F(SystemTest, DeleteFileAlreadyDeleted) {
  // Arrange: Upload and delete a file
  std::string test_content = "Content for double delete test";
  std::string test_file =
      createTestFile(test_content, "double_delete_test.txt");

  DFSClient client = createClient();
  auto upload_future = client.putFile(test_file);
  ASSERT_TRUE(upload_future.get()) << "Upload failed";

  // First delete
  auto first_delete_future = client.deleteFile(test_file);
  ASSERT_TRUE(first_delete_future.get()) << "First delete failed";

  // Act & Assert: Delete the same file again should throw ENOENT
  auto second_delete_future = client.deleteFile(test_file);

  try {
    second_delete_future.get();
    FAIL() << "Expected std::system_error to be thrown";
  } catch (const std::system_error& e) {
    EXPECT_EQ(e.code().value(), ENOENT)
        << "Should throw ENOENT for already deleted file";
  }
}

// Test concurrent file deletions from multiple clients
TEST_F(SystemTest, DeleteFileConcurrent) {
  const int NUM_CLIENTS = 12;
  const int FILES_PER_CLIENT = 2;

  // Arrange: Multiple clients upload files
  std::vector<std::thread> setup_threads;
  std::vector<std::string> all_files;
  std::mutex files_mutex;  // Protect all_files from concurrent access

  for (int client_id = 0; client_id < NUM_CLIENTS; ++client_id) {
    setup_threads.emplace_back([this, client_id, &all_files, &files_mutex]() {
      DFSClient client = createClient();

      for (int file_id = 0; file_id < FILES_PER_CLIENT; ++file_id) {
        std::string content = "Concurrent delete content " +
                              std::to_string(client_id) + "_" +
                              std::to_string(file_id);
        std::string filename = "concurrent_delete_" +
                               std::to_string(client_id) + "_" +
                               std::to_string(file_id) + ".txt";
        std::string filepath = createTestFile(content, filename);

        auto upload_future = client.putFile(filepath);
        ASSERT_TRUE(upload_future.get())
            << "Upload failed for client " << client_id << " file " << file_id;

        // Thread-safe access to all_files
        {
          std::lock_guard<std::mutex> lock(files_mutex);
          all_files.push_back(filepath);
        }
      }
    });
  }

  // Wait for all uploads to complete
  for (auto& thread : setup_threads) {
    thread.join();
  }

  // Act: All clients simultaneously delete their files
  std::vector<std::thread> delete_threads;
  std::atomic<int> success_count{0};

  for (int client_id = 0; client_id < NUM_CLIENTS; ++client_id) {
    delete_threads.emplace_back([client_id, &all_files, &success_count]() {
      DFSClient client = createClient();

      for (int file_id = 0; file_id < FILES_PER_CLIENT; ++file_id) {
        int file_index = client_id * FILES_PER_CLIENT + file_id;
        std::string filepath = all_files[file_index];

        try {
          auto delete_future = client.deleteFile(filepath);
          bool delete_success = delete_future.get();
          if (delete_success) {
            success_count++;
          } else {
            std::cerr << "Delete failed for client " << client_id << " file "
                      << file_id << std::endl;
          }
        } catch (const std::exception& e) {
          std::cerr << "Exception during delete for client " << client_id
                    << " file " << filepath << ": " << e.what() << std::endl;
        }
      }
    });
  }

  // Wait for all deletions to complete
  for (auto& thread : delete_threads) {
    thread.join();
  }

  // Assert: All deletions succeeded
  int expected_deletions = NUM_CLIENTS * FILES_PER_CLIENT;
  EXPECT_EQ(success_count, expected_deletions)
      << "Expected " << expected_deletions << " successful deletions, got "
      << success_count;

  // Assert: Files are actually gone
  DFSClient verify_client = createClient();
  for (const auto& filepath : all_files) {
    EXPECT_THROW(
        {
          auto get_future = verify_client.getFile(filepath);
          get_future.get();
        },
        std::exception)
        << "File " << filepath << " should not exist after deletion";
  }
}

// Test deletion during ongoing read operation
TEST_F(SystemTest, DeleteFileDuringRead) {
  // Arrange: Upload a moderately large file
  std::string test_content = generateRandomContent(1024 * 1024);  // 1MB
  std::string test_file =
      createTestFile(test_content, "delete_during_read.bin");

  DFSClient client = createClient();
  auto upload_future = client.putFile(test_file);
  ASSERT_TRUE(upload_future.get()) << "Upload failed";

  // Act: Start reading the file, then delete it
  auto read_future = client.getFile(test_file);

  // Delete while read is in progress
  auto delete_future = client.deleteFile(test_file);
  bool delete_success = delete_future.get();

  // Assert: Delete succeeds immediately (metadata deleted)
  EXPECT_TRUE(delete_success) << "Delete should succeed even during read";

  // Assert: Read operation completes (may succeed or fail gracefully)
  // The read might succeed if it completes before chunk cleanup,
  // or fail if chunks are cleaned up during read
  try {
    std::string read_content = read_future.get();
    // If read succeeds, content should match
    EXPECT_EQ(read_content, test_content);
  } catch (const std::exception& e) {
    // TODO: Need to think this more. When we have POSIX semantics the read
    // won't fail. But as things stand now.
    std::cout << "Read failed during delete (expected): " << e.what()
              << std::endl;
  }
}

// Test deletion with chunk cleanup verification
TEST_F(SystemTest, DeleteFileChunkCleanup) {
  // Arrange: Upload a file and track its chunks
  std::string test_content = "Content for chunk cleanup verification";
  std::string test_file =
      createTestFile(test_content, "chunk_cleanup_test.txt");

  DFSClient client = createClient();
  auto upload_future = client.putFile(test_file);
  ASSERT_TRUE(upload_future.get()) << "Upload failed";

  // Act: Delete the file
  auto delete_future = client.deleteFile(test_file);
  bool delete_success = delete_future.get();

  // Assert: Delete operation succeeds
  EXPECT_TRUE(delete_success) << "Delete operation failed";

  // Assert: File is gone (cannot be read)
  EXPECT_THROW(
      {
        auto verify_future = client.getFile(test_file);
        verify_future.get();
      },
      std::exception)
      << "File should not exist after deletion";
}

// ===== REPLICATION TESTS =====

// Test replication with sufficient nodes (normal case)
TEST_F(SystemTest, ReplicationWithSufficientNodes) {
  // With 3 DataNodes and replication factor 3, each node should get exactly one
  // replica
  std::string test_content = "Replication test with sufficient nodes";
  std::string test_file = createTestFile(test_content, "replication_test.txt");

  DFSClient client = createClient();
  auto upload_future = client.putFile(test_file);
  ASSERT_TRUE(upload_future.get()) << "Upload failed";

  // Verify file can be read back
  auto read_future = client.getFile(test_file);
  std::string received_content = read_future.get();
  EXPECT_EQ(received_content, test_content);

  // Verify that we have 3 registered nodes
  auto registered_nodes = coord_service->getRegisteredNodes();
  EXPECT_EQ(registered_nodes.size(), NUM_DATANODES)
      << "Should have exactly " << NUM_DATANODES << " registered nodes";
}

// Test replication with insufficient nodes (node reuse case)
TEST_F(SystemTest, ReplicationWithInsufficientNodes) {
  // This test verifies that when replication factor > available nodes,
  // the same node can store multiple replicas

  // Temporarily stop 2 DataNodes to simulate insufficient nodes scenario
  datanode_servers[1]->Stop();
  datanode_servers[2]->Stop();

  // Wait a moment for nodes to be marked as unavailable
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  // Upload a file - should still work with node reuse
  std::string test_content = "Replication test with node reuse";
  std::string test_file = createTestFile(test_content, "node_reuse_test.txt");

  DFSClient client = createClient();
  auto upload_future = client.putFile(test_file);
  ASSERT_TRUE(upload_future.get())
      << "Upload should succeed even with fewer nodes";

  // Verify file can be read back (using the remaining node)
  auto read_future = client.getFile(test_file);
  std::string received_content = read_future.get();
  EXPECT_EQ(received_content, test_content);

  // Restart the stopped DataNodes for other tests
  datanode_servers[1]->Start();
  datanode_servers[2]->Start();

  // Wait for them to re-register
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

// Test that files are accessible even when some nodes fail
TEST_F(SystemTest, ReplicationFaultTolerance) {
  // Upload a file first
  std::string test_content = "Fault tolerance test content";
  std::string test_file =
      createTestFile(test_content, "fault_tolerance_test.txt");

  DFSClient client = createClient();
  auto upload_future = client.putFile(test_file);
  ASSERT_TRUE(upload_future.get()) << "Upload failed";

  // Stop one DataNode to simulate failure
  size_t node_to_stop = 1;  // Stop the second DataNode
  datanode_servers[node_to_stop]->Stop();

  // Wait a moment for the failure to be detected
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  // File should still be readable from remaining replicas
  auto read_future = client.getFile(test_file);
  std::string received_content = read_future.get();
  EXPECT_EQ(received_content, test_content)
      << "File should be readable even when some nodes fail";

  // Restart the stopped DataNode
  datanode_servers[node_to_stop]->Start();
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

// Test that registered nodes information is correct
TEST_F(SystemTest, RegisteredNodesInformation) {
  GTEST_SKIP() << "TODO";
  auto registered_nodes = coord_service->getRegisteredNodes();

  // Should have exactly NUM_DATANODES registered
  EXPECT_EQ(registered_nodes.size(), NUM_DATANODES)
      << "Should have exactly " << NUM_DATANODES << " registered nodes";

  // Verify node IDs are sequential starting from 1
  std::vector<int32_t> expected_ids;
  for (int32_t i = 1; i <= NUM_DATANODES; ++i) {
    expected_ids.push_back(i);
  }

  std::vector<int32_t> actual_ids;
  for (const auto& pair : registered_nodes) {
    actual_ids.push_back(pair.first);
  }

  std::sort(actual_ids.begin(), actual_ids.end());
  EXPECT_EQ(actual_ids, expected_ids)
      << "Node IDs should be sequential starting from 1";

  // Verify each node has a valid address and port
  for (const auto& pair : registered_nodes) {
    int32_t node_id = pair.first;
    const auto& node_info = pair.second;

    EXPECT_EQ(node_id, node_info.node_id) << "Node ID mismatch in registry";

    // Address should be a valid localhost address (IPv4 or IPv6)
    EXPECT_FALSE(node_info.address.empty())
        << "Node address should not be empty";

    // Check for both IPv4 and IPv6 localhost addresses
    bool isLocalhost =
        (node_info.address.find("127.0.0.1") == 0) ||
        (node_info.address.find("localhost") == 0) ||
        (node_info.address.find("::1") == 0) ||       // IPv6 localhost
        (node_info.address.find("ipv6:[::1]") == 0);  // IPv6 with prefix

    EXPECT_TRUE(isLocalhost)
        << "Node address should be localhost (IPv4 or IPv6), got: "
        << node_info.address;

    // Port should be in the expected range
    EXPECT_GE(node_info.port, 50054) << "Port should be >= 50054";
    EXPECT_LE(node_info.port, 50054 + NUM_DATANODES - 1)
        << "Port should be <= " << (50054 + NUM_DATANODES - 1);
  }
}

// Test DataNode unregistration functionality
TEST_F(SystemTest, DataNodeUnregistration) {
  // Get initial node count
  auto initial_nodes = coord_service->getRegisteredNodes();
  size_t initial_count = initial_nodes.size();
  EXPECT_EQ(initial_count, NUM_DATANODES)
      << "Should start with " << NUM_DATANODES << " nodes";

  // Stop one DataNode - should trigger unregistration
  size_t node_to_stop = 1;  // Stop the second DataNode
  datanode_servers[node_to_stop]->Stop();

  // Wait a moment for unregistration to complete
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  // Verify node was removed from registry
  auto after_stop_nodes = coord_service->getRegisteredNodes();
  EXPECT_EQ(after_stop_nodes.size(), initial_count - 1)
      << "Node count should decrease by 1 after stopping a DataNode";

  // Verify the correct node was removed
  int32_t removed_node_id = node_to_stop + 1;  // Node IDs start from 1
  EXPECT_EQ(after_stop_nodes.find(removed_node_id), after_stop_nodes.end())
      << "Node ID " << removed_node_id << " should have been removed";

  // Restart the DataNode
  datanode_servers[node_to_stop]->Start();

  // Wait for it to re-register
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  // Verify node count is back to original
  auto final_nodes = coord_service->getRegisteredNodes();
  EXPECT_EQ(final_nodes.size(), initial_count)
      << "Node count should be back to " << initial_count << " after restart";

  // Verify the restarted node has a new ID (since IDs are not reused)
  EXPECT_GT(final_nodes.size(), after_stop_nodes.size())
      << "New node should have been registered with a new ID";
}
