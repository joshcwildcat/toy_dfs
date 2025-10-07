cmake_minimum_required(VERSION 3.16)
project(ToyDFS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# === Build Configuration ===
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

set(CMAKE_BUILD_TYPE_AVAILABLE "Debug;Release;MinSizeRel;RelWithDebInfo" CACHE STRING "Available build types" FORCE)

# Set compiler flags based on build type
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -ggdb -DDEBUG" CACHE STRING "Debug flags" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG" CACHE STRING "Release flags" FORCE)
set(CMAKE_CXX_FLAGS_MINSIZEREL "-Os -DNDEBUG" CACHE STRING "MinSizeRel flags" FORCE)
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG" CACHE STRING "RelWithDebInfo flags" FORCE)

# Curated compiler flags - valuable warnings that catch real bugs
set(COMMON_COMPILE_FLAGS "-Wall"
                        "-Wextra"
                        "-Werror"
                        "-Wuninitialized"
                        "-Wreturn-type"
                        "-Wparentheses"
                        "-Wformat=2"
                        "-Wunused-result"
                        "-Wsign-compare"
                        "-Wsizeof-pointer-memaccess"
                        # Suppress noisy clang-specific warnings from system headers
                        "-Wno-error=nullability-extension"
                        "-Wno-error=gcc-compat"
                        "-Wno-error=variadic-macros"
                        "-Wno-error=invalid-offsetof"
                        "-Wno-error=gnu-statement-expression"
                        "-Wno-error=unused-parameter")

# === Testing Configuration ===
option(ENABLE_TESTING "Enable testing" ON)
option(ENABLE_COVERAGE "Enable code coverage reporting" OFF)
option(ENABLE_SANITIZERS "Enable sanitizers for debugging" OFF)

if(ENABLE_TESTING)
    enable_testing()

    # Coverage reporting (if supported)
    if(ENABLE_COVERAGE)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
            # GNU gcov coverage
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            # Clang llvm-cov coverage
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-instr-generate -fcoverage-mapping")
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-instr-generate -fcoverage-mapping")
        endif()
    endif()

    # Sanitizers for test builds
    if(ENABLE_SANITIZERS)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize=undefined")
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address -fsanitize=undefined")
        endif()
    endif()
endif()

# === Dependencies ===
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

# === Testing ===
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# === Path Configuration ===
set(TOYDFS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(TOYDFS_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(PROTO_SRC_DIR "${TOYDFS_SOURCE_DIR}/proto")
set(GENERATED_DIR "${TOYDFS_BINARY_DIR}/generated")

# Ensure all build artifacts go to build directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${TOYDFS_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${TOYDFS_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${TOYDFS_BINARY_DIR}/lib")
set(CMAKE_GENERATED_DIR "${GENERATED_DIR}")

set(COORDINATOR_PROTO_FILE "${PROTO_SRC_DIR}/coordinator/coordinator.proto")
set(DATANODE_PROTO_FILE "${PROTO_SRC_DIR}/datanode/datanode.proto")
file(MAKE_DIRECTORY ${GENERATED_DIR})
file(MAKE_DIRECTORY ${GENERATED_DIR}/coordinator)
file(MAKE_DIRECTORY ${GENERATED_DIR}/datanode)

set(COORDINATOR_GENERATED_PROTO_SRC "${GENERATED_DIR}/coordinator/coordinator.pb.cc")
set(COORDINATOR_GENERATED_PROTO_HDR "${GENERATED_DIR}/coordinator/coordinator.pb.h")
set(COORDINATOR_GENERATED_GRPC_SRC "${GENERATED_DIR}/coordinator/coordinator.grpc.pb.cc")
set(COORDINATOR_GENERATED_GRPC_HDR "${GENERATED_DIR}/coordinator/coordinator.grpc.pb.h")

set(DATANODE_GENERATED_PROTO_SRC "${GENERATED_DIR}/datanode/datanode.pb.cc")
set(DATANODE_GENERATED_PROTO_HDR "${GENERATED_DIR}/datanode/datanode.pb.h")
set(DATANODE_GENERATED_GRPC_SRC "${GENERATED_DIR}/datanode/datanode.grpc.pb.cc")
set(DATANODE_GENERATED_GRPC_HDR "${GENERATED_DIR}/datanode/datanode.grpc.pb.h")

add_custom_command(
  OUTPUT "${COORDINATOR_GENERATED_PROTO_SRC}" "${COORDINATOR_GENERATED_PROTO_HDR}" "${COORDINATOR_GENERATED_GRPC_SRC}" "${COORDINATOR_GENERATED_GRPC_HDR}"
  COMMAND protobuf::protoc
  ARGS --grpc_out "${GENERATED_DIR}"
       --cpp_out "${GENERATED_DIR}"
       --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
       -I "${PROTO_SRC_DIR}"
       "${COORDINATOR_PROTO_FILE}"
  DEPENDS "${COORDINATOR_PROTO_FILE}"
)

add_custom_command(
  OUTPUT "${DATANODE_GENERATED_PROTO_SRC}" "${DATANODE_GENERATED_PROTO_HDR}" "${DATANODE_GENERATED_GRPC_SRC}" "${DATANODE_GENERATED_GRPC_HDR}"
  COMMAND protobuf::protoc
  ARGS --grpc_out "${GENERATED_DIR}"
       --cpp_out "${GENERATED_DIR}"
       --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
       -I "${PROTO_SRC_DIR}"
       "${DATANODE_PROTO_FILE}"
  DEPENDS "${DATANODE_PROTO_FILE}"
)

add_library(dfs_proto STATIC
    "${COORDINATOR_GENERATED_PROTO_SRC}"
    "${COORDINATOR_GENERATED_GRPC_SRC}"
    "${DATANODE_GENERATED_PROTO_SRC}"
    "${DATANODE_GENERATED_GRPC_SRC}"
)

target_include_directories(dfs_proto SYSTEM PUBLIC
    "${GENERATED_DIR}"
)

target_include_directories(dfs_proto PUBLIC
    "${PROTO_SRC_DIR}"
)

target_link_libraries(dfs_proto
    PUBLIC
    gRPC::grpc++
    protobuf::libprotobuf
)
target_compile_options(dfs_proto PRIVATE ${COMMON_COMPILE_FLAGS})
set_target_properties(dfs_proto PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

# Create service libraries for testing
add_library(coordinator_service_lib STATIC coordinator/src/coordinator_service_impl.cpp)
target_link_libraries(coordinator_service_lib PUBLIC dfs_proto gRPC::grpc++ protobuf::libprotobuf)
target_include_directories(coordinator_service_lib SYSTEM PUBLIC ${GENERATED_DIR})
target_include_directories(coordinator_service_lib PUBLIC ${PROTO_SRC_DIR} ${TOYDFS_SOURCE_DIR}/coordinator/include)
target_compile_options(coordinator_service_lib PRIVATE ${COMMON_COMPILE_FLAGS})
set_target_properties(coordinator_service_lib PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

add_library(test_datanode_service_lib STATIC datanode/src/datanode_service_impl.cpp datanode/src/datanode_utils.cpp)
target_link_libraries(test_datanode_service_lib PUBLIC dfs_proto gRPC::grpc++ protobuf::libprotobuf)
target_include_directories(test_datanode_service_lib SYSTEM PUBLIC ${GENERATED_DIR})
target_include_directories(test_datanode_service_lib PUBLIC ${PROTO_SRC_DIR} ${TOYDFS_SOURCE_DIR}/datanode/include)
target_compile_options(test_datanode_service_lib PRIVATE ${COMMON_COMPILE_FLAGS})
set_target_properties(test_datanode_service_lib PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

# DFSClient library
add_library(dfs_client_lib STATIC client/src/dfs_client.cpp)
target_link_libraries(dfs_client_lib PUBLIC dfs_proto gRPC::grpc++ protobuf::libprotobuf)
target_include_directories(dfs_client_lib SYSTEM PUBLIC ${GENERATED_DIR})
target_include_directories(dfs_client_lib PUBLIC ${PROTO_SRC_DIR} ${TOYDFS_SOURCE_DIR}/client/include)
target_compile_options(dfs_client_lib PRIVATE ${COMMON_COMPILE_FLAGS})
set_target_properties(dfs_client_lib PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

# === Subdirectories ===
add_subdirectory(server)
add_subdirectory(coordinator)
add_subdirectory(datanode)
add_subdirectory(client)
add_subdirectory(test)
