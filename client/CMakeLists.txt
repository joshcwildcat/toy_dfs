add_executable(client
    src/main.cpp
)

target_link_libraries(client
    PRIVATE
    dfs_client_lib
    dfs_proto
    gRPC::grpc++
    protobuf::libprotobuf
)

target_include_directories(client PRIVATE
    ${TOYDFS_SOURCE_DIR}/client/include
    ${TOYDFS_BINARY_DIR}
)
