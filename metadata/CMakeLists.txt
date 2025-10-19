set(METADATA_PROTO_SRC_DIR "${TOYDFS_SOURCE_DIR}/metadata/proto")
set(METADATA_PROTO_FILE "${METADATA_PROTO_SRC_DIR}/metadata.proto")
file(MAKE_DIRECTORY "${GENERATED_DIR}/metadata")

set(METADATA_GENERATED_PROTO_SRC "${GENERATED_DIR}/metadata/metadata.pb.cc")
set(METADATA_GENERATED_PROTO_HDR "${GENERATED_DIR}/metadata/metadata.pb.h")
set(METADATA_GENERATED_GRPC_SRC "${GENERATED_DIR}/metadata/metadata.grpc.pb.cc")
set(METADATA_GENERATED_GRPC_HDR "${GENERATED_DIR}/metadata/metadata.grpc.pb.h")

add_custom_command(
  OUTPUT "${METADATA_GENERATED_PROTO_SRC}" "${METADATA_GENERATED_PROTO_HDR}" "${METADATA_GENERATED_GRPC_SRC}" "${METADATA_GENERATED_GRPC_HDR}"
  COMMAND protobuf::protoc
  ARGS --grpc_out "${GENERATED_DIR}/metadata"
       --cpp_out "${GENERATED_DIR}/metadata"
       --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
       -I "${METADATA_PROTO_SRC_DIR}"
       "${METADATA_PROTO_FILE}"
  DEPENDS "${METADATA_PROTO_FILE}"
)

add_library(dfs_metadata_proto STATIC
    "${METADATA_GENERATED_PROTO_SRC}"
    "${METADATA_GENERATED_GRPC_SRC}"
)

target_include_directories(dfs_metadata_proto SYSTEM PUBLIC
    "${GENERATED_DIR}/metadata"
)

target_include_directories(dfs_metadata_proto PUBLIC
    "${METADATA_PROTO_SRC_DIR}"
)

target_link_libraries(dfs_metadata_proto
    PUBLIC
    gRPC::grpc++
    protobuf::libprotobuf
)
target_compile_options(dfs_metadata_proto PRIVATE ${COMMON_COMPILE_FLAGS})
set_target_properties(dfs_metadata_proto PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

