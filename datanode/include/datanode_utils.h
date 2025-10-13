#pragma once
#include <string>

#include "datanode_config.h"

// Utility functions for DataNode operations
void create_directories_recursively(const std::string& path);
std::string get_chunk_path(const std::string& chunk_id,
                           const DataNodeConfig& config = DataNodeConfig{});
