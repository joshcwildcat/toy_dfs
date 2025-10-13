#pragma once
#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

struct ChunkInfo {
  std::string chunk_id;
  std::vector<int32_t> node_ids;   // Integer node IDs for replication
  int32_t replication_factor = 3;  // Desired replication level
};

struct FileMeta {
  int64_t size;
  int chunk_size;
  std::vector<ChunkInfo> chunks;
};

struct PathEntry {
  bool is_file;
  FileMeta file_meta;
};

class MetadataStore {
public:
  // Core data structures
  std::unordered_map<std::string, PathEntry> entries_;  // Active files only
  std::deque<
      std::tuple<std::string, FileMeta, std::chrono::steady_clock::time_point>>
      deleted_entries_;  // Files pending chunk cleanup

  // Concurrency control - allows multiple readers, exclusive writer
  mutable std::shared_mutex mutex_;

  // Configuration
  static constexpr auto TOMBSTONE_LIFETIME = std::chrono::minutes(5);
  static constexpr size_t MAX_DELETED_ENTRIES = 10000;

  // Statistics for monitoring
  mutable std::atomic<size_t> read_count_{0};
  mutable std::atomic<size_t> write_count_{0};
  mutable std::atomic<size_t> tombstone_count_{0};

  // Write operation - requires exclusive access
  void putFile(const std::string& path, const FileMeta& meta) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    entries_[path] = PathEntry{true, meta};
    write_count_.fetch_add(1, std::memory_order_relaxed);
  }

  // Read operation - allows concurrent access
  FileMeta* getFile(const std::string& path) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    read_count_.fetch_add(1, std::memory_order_relaxed);
    auto it = entries_.find(path);
    if (it == entries_.end() || !it->second.is_file)
      return nullptr;
    return &it->second.file_meta;
  }

  // Write operation - requires exclusive access
  void deletePath(const std::string& path) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto it = entries_.find(path);
    if (it != entries_.end() && it->second.is_file) {
      // Move file to deleted_entries for cleanup
      FileMeta deleted_meta = std::move(it->second.file_meta);
      auto delete_time = std::chrono::steady_clock::now();
      deleted_entries_.emplace_back(path, std::move(deleted_meta), delete_time);
      entries_.erase(it);  // Remove from active entries

      // Manage memory usage
      if (deleted_entries_.size() > MAX_DELETED_ENTRIES) {
        cleanupOldTombstones();
      }

      write_count_.fetch_add(1, std::memory_order_relaxed);
      tombstone_count_.fetch_add(1, std::memory_order_relaxed);
    }
  }

  // Read operation - allows concurrent access
  bool exists(const std::string& path) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    read_count_.fetch_add(1, std::memory_order_relaxed);
    return entries_.find(path) != entries_.end();
  }

  // Enhanced cleanup with better performance
  void cleanupTombstones(std::function<void(const FileMeta&)> chunkDeleter) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    cleanupOldTombstonesInternal(chunkDeleter);
  }

  // Statistics for monitoring
  struct Statistics {
    size_t read_count;
    size_t write_count;
    size_t tombstone_count;
    size_t active_entries;
    size_t deleted_entries;
  };

  Statistics getStatistics() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return {read_count_.load(std::memory_order_relaxed),
            write_count_.load(std::memory_order_relaxed),
            tombstone_count_.load(std::memory_order_relaxed), entries_.size(),
            deleted_entries_.size()};
  }

private:
  // Internal cleanup implementation
  void cleanupOldTombstones() {
    auto now = std::chrono::steady_clock::now();

    // Remove old tombstones from front of deque
    while (!deleted_entries_.empty()) {
      auto& [path, meta, delete_time] = deleted_entries_.front();
      auto age = now - delete_time;
      if (age <= TOMBSTONE_LIFETIME)
        break;

      deleted_entries_.pop_front();
      tombstone_count_.fetch_sub(1, std::memory_order_relaxed);
    }
  }

  void cleanupOldTombstonesInternal(
      std::function<void(const FileMeta&)> chunkDeleter) {
    auto now = std::chrono::steady_clock::now();

    // Process tombstones that are ready for cleanup
    for (auto it = deleted_entries_.begin(); it != deleted_entries_.end();) {
      auto& [path, meta, delete_time] = *it;
      auto age = now - delete_time;
      if (age > TOMBSTONE_LIFETIME) {
        // Time to cleanup: delete chunks and remove from deleted_entries
        if (chunkDeleter) {
          chunkDeleter(meta);
        }
        it = deleted_entries_.erase(it);
        tombstone_count_.fetch_sub(1, std::memory_order_relaxed);
        continue;
      }
      ++it;
    }
  }
};
