#include "datanode_config.h"
#include <random>
#include <sstream>

DataNodeConfig DataNodeConfig::CreateUnique(const std::string& base_path,
                                            const std::string& unique_suffix) {
  DataNodeConfig config;
  config.chunk_root_path = base_path;

  if (unique_suffix.empty()) {
    // Generate a random unique identifier
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> distrib(1000, 9999);
    config.node_id = "node_" + std::to_string(distrib(gen));
  } else {
    config.node_id = unique_suffix;
  }

  return config;
}
