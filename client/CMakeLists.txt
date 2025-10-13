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

add_library(dfs_client_lib STATIC src/dfs_client.cpp)
target_link_libraries(dfs_client_lib PUBLIC dfs_proto gRPC::grpc++ protobuf::libprotobuf)
target_include_directories(dfs_client_lib SYSTEM PUBLIC ${GENERATED_DIR})
target_include_directories(dfs_client_lib PUBLIC ${PROTO_SRC_DIR} ${TOYDFS_SOURCE_DIR}/client/include)
target_compile_options(dfs_client_lib PRIVATE ${COMMON_COMPILE_FLAGS})
set_target_properties(dfs_client_lib PROPERTIES
    CXX_EXTENSIONS OFF  
    POSITION_INDEPENDENT_CODE ON
)
