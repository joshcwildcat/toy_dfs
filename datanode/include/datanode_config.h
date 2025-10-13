#pragma once
#include <string>

/**
 * Configuration structure for DataNode chunk storage settings.
 * Allows customization of chunk root path and node identification.
 */
struct DataNodeConfig {
  std::string chunk_root_path = "/tmp/dfs_chunks";
  std::string node_id =
      "";  // Optional node identifier for unique path generation

  /**
   * Create a unique configuration for each DataNode instance.
   * @param base_path Base path for chunk storage
   * @param unique_suffix Optional unique identifier (auto-generated if empty)
   */
  static DataNodeConfig
  CreateUnique(const std::string& base_path = "/tmp/dfs_chunks",
               const std::string& unique_suffix = "");
};
