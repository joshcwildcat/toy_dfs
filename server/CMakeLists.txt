# Server library - builds server-related functionality
# Explicitly list all source files for maintainability and clarity

set(SERVER_SOURCES
    src/datanode_server.cpp
    # Add other server source files here as they are created
)

add_library(datanode_server_lib STATIC ${SERVER_SOURCES})

target_link_libraries(datanode_server_lib
    PUBLIC
    dfs_proto
    gRPC::grpc++
    protobuf::libprotobuf
)

target_include_directories(datanode_server_lib PUBLIC
    ${TOYDFS_SOURCE_DIR}/server/include
    ${TOYDFS_SOURCE_DIR}/coordinator/include
    ${TOYDFS_SOURCE_DIR}/datanode/include
    ${TOYDFS_BINARY_DIR}
)

target_compile_options(datanode_server_lib PRIVATE ${COMMON_COMPILE_FLAGS})
set_target_properties(datanode_server_lib PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)
