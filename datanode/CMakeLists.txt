add_library(datanode_service_lib
    src/datanode_service_impl.cpp
    src/datanode_utils.cpp
)

target_link_libraries(datanode_service_lib
    PRIVATE
    dfs_proto
    gRPC::grpc++
    protobuf::libprotobuf
)

target_include_directories(datanode_service_lib PRIVATE
    ${TOYDFS_SOURCE_DIR}/datanode/include
    ${TOYDFS_BINARY_DIR}
)

add_executable(datanode
    src/main.cpp
)

target_link_libraries(datanode
    PRIVATE
    datanode_service_lib
    dfs_proto
    gRPC::grpc++
    protobuf::libprotobuf
)

target_include_directories(datanode PRIVATE
    ${TOYDFS_SOURCE_DIR}/datanode/include
    ${TOYDFS_BINARY_DIR}
    ${TOYDFS_SOURCE_DIR}/server/include
)
