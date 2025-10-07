#include "datanode_utils.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

void create_directories_recursively(const std::string& path) {
    size_t pos = 0;
    std::string current = "";
    while ((pos = path.find('/', pos)) != std::string::npos) {
        current = path.substr(0, pos);
        if (!current.empty() && mkdir(current.c_str(), 0755) != 0 && errno != EEXIST) {
            throw std::runtime_error("Failed to create directory: " + current);
        }
        pos++;
    }
    if (mkdir(path.c_str(), 0755) != 0 && errno != EEXIST) {
        throw std::runtime_error("Failed to create directory: " + path);
    }
}

std::string get_chunk_path(const std::string& chunk_id) {
    size_t pos = chunk_id.find('_');
    if (pos == std::string::npos || chunk_id.substr(0, pos) != "chunk") return chunk_id;
    int N = std::stoi(chunk_id.substr(pos + 1));
    std::string base_path = "/tmp/dfs_chunks";
    //TODO Think about this
    int group_size = 1000;
    int level3 = N / (group_size * group_size);
    int level2 = (N / group_size) % group_size;
    //int level1 = N % group_size;
    std::string dir_path = base_path + "/" + std::to_string(level3) + "/" + std::to_string(level2);
    try {
        create_directories_recursively(dir_path);
    } catch (const std::runtime_error& e) {
        // Directory creation failed, but continue - file open will fail if needed
    }
    return dir_path + "/" + std::to_string(N) + ".cfile";
}
