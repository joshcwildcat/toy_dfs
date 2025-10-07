#pragma once
#include <string>

// Utility functions for DataNode operations
void create_directories_recursively(const std::string& path);
std::string get_chunk_path(const std::string& chunk_id);
