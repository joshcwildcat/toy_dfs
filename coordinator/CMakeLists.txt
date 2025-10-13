add_executable(coordinator
    src/main.cpp
)

target_link_libraries(coordinator
    PRIVATE
    coordinator_service_lib
    datanode_client_lib
    dfs_proto
    gRPC::grpc++
    protobuf::libprotobuf
)

target_include_directories(coordinator PRIVATE
    ${TOYDFS_SOURCE_DIR}/coordinator/include
    ${TOYDFS_SOURCE_DIR}/server/include
    ${TOYDFS_BINARY_DIR}
)

# Create service libraries for testing
add_library(coordinator_service_lib STATIC
    src/coordinator_service_impl.cpp
)
target_link_libraries(coordinator_service_lib PUBLIC dfs_proto gRPC::grpc++ protobuf::libprotobuf)
target_include_directories(coordinator_service_lib SYSTEM PUBLIC ${GENERATED_DIR})
target_include_directories(coordinator_service_lib PUBLIC ${PROTO_SRC_DIR}
    ${TOYDFS_SOURCE_DIR}/coordinator/include
    ${TOYDFS_SOURCE_DIR}/datanode/include
)
target_compile_options(coordinator_service_lib PRIVATE ${COMMON_COMPILE_FLAGS})
set_target_properties(coordinator_service_lib PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)      

