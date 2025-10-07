#include <gtest/gtest.h>
#include "metadata_store.h"

// Test fixture for MetadataStore
class MetadataStoreTest : public ::testing::Test {
protected:
    MetadataStore store;
};

// Test putFile and getFile
TEST_F(MetadataStoreTest, PutAndGetFile) {
    FileMeta meta;
    meta.size = 1024;
    meta.chunk_size = 512;
    ChunkInfo chunk;
    chunk.chunk_id = "chunk_0";
    chunk.locations = {"node1"};
    meta.chunks.push_back(chunk);

    store.putFile("test_file", meta);

    FileMeta* retrieved = store.getFile("test_file");
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->size, 1024);
    EXPECT_EQ(retrieved->chunk_size, 512);
    EXPECT_EQ(retrieved->chunks.size(), 1);
    EXPECT_EQ(retrieved->chunks[0].chunk_id, "chunk_0");
}

// Test getFile for non-existent file
TEST_F(MetadataStoreTest, GetNonExistentFile) {
    FileMeta* retrieved = store.getFile("non_existent");
    EXPECT_EQ(retrieved, nullptr);
}

// Test exists
TEST_F(MetadataStoreTest, Exists) {
    FileMeta meta;
    meta.size = 100;
    store.putFile("existing_file", meta);

    EXPECT_TRUE(store.exists("existing_file"));
    EXPECT_FALSE(store.exists("non_existing_file"));
}

// Test concurrent reads
TEST_F(MetadataStoreTest, ConcurrentReads) {
    FileMeta meta;
    meta.size = 100;
    store.putFile("concurrent_file", meta);

    // Launch multiple reader threads
    const int num_readers = 10;
    std::vector<std::thread> readers;
    std::atomic<int> successful_reads{0};

    for (int i = 0; i < num_readers; ++i) {
        readers.emplace_back([&]() {
            for (int j = 0; j < 100; ++j) {
                if (store.exists("concurrent_file")) {
                    successful_reads++;
                }
                FileMeta* retrieved = store.getFile("concurrent_file");
                if (retrieved && retrieved->size == 100) {
                    successful_reads++;
                }
            }
        });
    }

    // Wait for all readers to complete
    for (auto& reader : readers) {
        reader.join();
    }

    EXPECT_EQ(successful_reads, num_readers * 200);  // All reads should succeed
}

// Test statistics
TEST_F(MetadataStoreTest, Statistics) {
    FileMeta meta;
    meta.size = 100;
    store.putFile("stats_file", meta);

    // Perform some operations
    store.getFile("stats_file");
    store.getFile("stats_file");
    store.exists("stats_file");

    auto stats = store.getStatistics();
    EXPECT_GE(stats.read_count, 3);
    EXPECT_EQ(stats.write_count, 1);  // One putFile
    EXPECT_EQ(stats.active_entries, 1);
    EXPECT_EQ(stats.tombstone_count, 0);
}

// Test deletePath
TEST_F(MetadataStoreTest, DeletePath) {
    FileMeta meta;
    meta.size = 100;
    store.putFile("file_to_delete", meta);

    EXPECT_TRUE(store.exists("file_to_delete"));
    store.deletePath("file_to_delete");
    EXPECT_FALSE(store.exists("file_to_delete"));
}

// Test deletePath for non-existent file
TEST_F(MetadataStoreTest, DeleteNonExistentPath) {
    // Should not crash
    store.deletePath("non_existent");
    EXPECT_FALSE(store.exists("non_existent"));
}

// Test overwriting file
TEST_F(MetadataStoreTest, OverwriteFile) {
    FileMeta meta1;
    meta1.size = 100;
    store.putFile("file", meta1);

    FileMeta meta2;
    meta2.size = 200;
    store.putFile("file", meta2);

    FileMeta* retrieved = store.getFile("file");
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->size, 200);
}

// Test multiple files
TEST_F(MetadataStoreTest, MultipleFiles) {
    FileMeta meta1, meta2;
    meta1.size = 100;
    meta2.size = 200;

    store.putFile("file1", meta1);
    store.putFile("file2", meta2);

    EXPECT_TRUE(store.exists("file1"));
    EXPECT_TRUE(store.exists("file2"));

    FileMeta* r1 = store.getFile("file1");
    FileMeta* r2 = store.getFile("file2");

    ASSERT_NE(r1, nullptr);
    ASSERT_NE(r2, nullptr);
    EXPECT_EQ(r1->size, 100);
    EXPECT_EQ(r2->size, 200);
}
