add_executable(coordinator
    src/main.cpp
)

target_link_libraries(coordinator
    PRIVATE
    coordinator_service_lib
    dfs_proto
    gRPC::grpc++
    protobuf::libprotobuf
)

target_include_directories(coordinator PRIVATE
    ${TOYDFS_SOURCE_DIR}/coordinator/include
    ${TOYDFS_SOURCE_DIR}/server/include
    ${TOYDFS_BINARY_DIR}
)
