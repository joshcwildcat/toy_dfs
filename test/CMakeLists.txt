# === Path Configuration ===
# Use the same path variables as the root CMakeLists.txt for consistency
set(TOYDFS_SOURCE_DIR "${CMAKE_SOURCE_DIR}")
set(TOYDFS_BINARY_DIR "${CMAKE_BINARY_DIR}")
set(INCLUDES ${TOYDFS_SOURCE_DIR}/client/include
    ${TOYDFS_SOURCE_DIR}/coordinator/include
    ${TOYDFS_SOURCE_DIR}/datanode/include
    ${TOYDFS_SOURCE_DIR}/server/include
    ${TOYDFS_SOURCE_DIR}/test/include
    ${TOYDFS_BINARY_DIR}
    ${TOYDFS_SOURCE_DIR}
)

# Generated files should be treated as system headers to avoid gRPC warnings
set(GENERATED_INCLUDES ${TOYDFS_BINARY_DIR}/generated)

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

# === Test Configuration ===
option(TEST_VERBOSE_OUTPUT "Enable verbose test output" OFF)
option(TEST_WITH_COVERAGE "Include coverage flags in tests" OFF)

if(TEST_VERBOSE_OUTPUT)
    set(CMAKE_VERBOSE_MAKEFILE ON)
    set(CMAKE_CTEST_COMMAND "${CMAKE_CTEST_COMMAND} --verbose")
endif()

if(TEST_WITH_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
    endif()
endif()

add_executable(test_metadata_store src/test_metadata_store.cpp)
target_link_libraries(test_metadata_store
    PRIVATE
    gtest_main
    dfs_proto
)
target_include_directories(test_metadata_store SYSTEM PRIVATE ${GENERATED_INCLUDES})
target_include_directories(test_metadata_store PRIVATE ${INCLUDES})
target_compile_options(test_metadata_store PRIVATE ${COMMON_COMPILE_FLAGS})

add_executable(test_coordinator_service src/test_coordinator_service.cpp)
target_link_libraries(test_coordinator_service
    PRIVATE
    gtest_main
    gmock
    coordinator_service_lib
    datanode_client_lib
)
target_include_directories(test_coordinator_service SYSTEM PRIVATE ${GENERATED_INCLUDES})
target_include_directories(test_coordinator_service PRIVATE ${INCLUDES})
target_compile_definitions(test_coordinator_service PRIVATE TEST_BUILD)
target_compile_options(test_coordinator_service PRIVATE ${COMMON_COMPILE_FLAGS})

add_executable(test_datanode_service src/test_datanode_service.cpp)
target_link_libraries(test_datanode_service
    PRIVATE
    gtest_main
    datanode_service_lib
)
target_include_directories(test_datanode_service SYSTEM PRIVATE ${GENERATED_INCLUDES})
target_include_directories(test_datanode_service PRIVATE ${INCLUDES})
target_compile_options(test_datanode_service PRIVATE ${COMMON_COMPILE_FLAGS})

add_executable(system_test src/system_test.cpp)
target_link_libraries(system_test
    PRIVATE
    gtest_main
    coordinator_service_lib
    datanode_client_lib
    datanode_service_lib
    datanode_server_lib
    dfs_client_lib
    dfs_proto
    gRPC::grpc++
    protobuf::libprotobuf
)
target_include_directories(system_test SYSTEM PRIVATE ${GENERATED_INCLUDES})
target_include_directories(system_test PRIVATE ${INCLUDES})
target_compile_definitions(system_test PRIVATE TEST_BUILD)
target_compile_options(system_test PRIVATE ${COMMON_COMPILE_FLAGS})

include(GoogleTest)

# Discover tests for all test executables. Use PROCESSES so a new process isn't forked for each test method. 
gtest_discover_tests(test_metadata_store  PROPERTIES ENVIRONMENT "LLVM_PROFILE_FILE=prof_%p_%h.profraw")
gtest_discover_tests(test_datanode_service PROPERTIES ENVIRONMENT "LLVM_PROFILE_FILE=prof_%p_%h.profraw")
gtest_discover_tests(test_coordinator_service PROPERTIES ENVIRONMENT "LLVM_PROFILE_FILE=prof_%p_%h.profraw")

# We aren't using gtest_discover_tests for the system_test because CMAKE will run each test function in a seperate process.
# The system_test will setup fixtures to run a coordinator and data_nodes for the duration of all the tests mimicing a more
# real world seneario. However, this makes it appear from CMAKEs perspective that only one test was run. So we configure gtest
# to report all tests passed, failed, and skipped in an xml file. run_test_and_parse.py simply parses the data and prints it out.
# TODO: Configure to print output even when all tests pass.
add_test(NAME system_test
        COMMAND python3 ${CMAKE_SOURCE_DIR}/test/run_test_and_parse.py
            $<TARGET_FILE:system_test>
            ${CMAKE_BINARY_DIR}/system_test.xml
)
set_tests_properties(system_test PROPERTIES
    ENVIRONMENT "LLVM_PROFILE_FILE=prof_%p_%h.profraw"
)


# === Coverage Reporting ===
if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        # GNU gcov coverage with lcov
        find_program(LCOV_PATH lcov)
        find_program(GENHTML_PATH genhtml)

        if(LCOV_PATH AND GENHTML_PATH)
            add_custom_target(coverage
                COMMAND ${LCOV_PATH} --capture --directory ${CMAKE_BINARY_DIR} --output-file coverage.info
                COMMAND ${LCOV_PATH} --remove coverage.info '/usr/*' '*/test/*' '*/build/*' --output-file coverage.info
                COMMAND ${LCOV_PATH} --list coverage.info
                COMMAND ${GENHTML_PATH} coverage.info --output-directory coverage_report
                COMMENT "Generating GNU gcov code coverage report"
                VERBATIM
            )

            add_custom_target(coverage_clean
                COMMAND ${LCOV_PATH} --zerocounters --directory ${CMAKE_BINARY_DIR}
                COMMAND rm -f coverage.info
                COMMAND rm -rf coverage_report
                COMMENT "Cleaning GNU gcov coverage data"
                VERBATIM
            )
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        #TODO: This isn't working quite right yet
        # Clang llvm-cov coverage
        find_program(LLVM_PROFDATA_PATH llvm-profdata)
        find_program(LLVM_COV_PATH llvm-cov)

        if(LLVM_PROFDATA_PATH AND LLVM_COV_PATH)
            add_custom_target(coverage
                # Step 1: Ensure all test executables are built
                DEPENDS test_metadata_store test_coordinator_service test_datanode_service system_test

                # Step 2: Run all tests to generate coverage data (.profraw files)
                COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure

                # Step 3: Merge coverage data from all profraw files
                COMMAND find ${CMAKE_BINARY_DIR} -name "*.profraw" -exec ${LLVM_PROFDATA_PATH} merge -sparse {} -o ${CMAKE_BINARY_DIR}/coverage.profdata \;

                # Step 4: Generate coverage reports
                COMMAND bash -c "for bin in ${CMAKE_BINARY_DIR}/bin/test_*; do ${LLVM_COV_PATH} report \$bin -instr-profile=${CMAKE_BINARY_DIR}/coverage.profdata -ignore-filename-regex='test/.*' -ignore-filename-regex='build/.*'; done"
                COMMAND bash -c "for bin in ${CMAKE_BINARY_DIR}/bin/test_*; do ${LLVM_COV_PATH} show \$bin -instr-profile=${CMAKE_BINARY_DIR}/coverage.profdata -ignore-filename-regex='test/.*' -ignore-filename-regex='build/.*' -format=html -output-dir=coverage_report; done"

                COMMENT "Building tests, running tests, and generating Clang llvm-cov code coverage report"
                VERBATIM
            )

            add_custom_target(coverage_clean
                COMMAND rm -f ${CMAKE_BINARY_DIR}/*.profraw ${CMAKE_BINARY_DIR}/coverage.profdata
                COMMAND rm -rf coverage_report
                COMMENT "Cleaning Clang llvm-cov coverage data"
                VERBATIM
            )
        endif()
    endif()
endif()

# === Test Utilities ===
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_metadata_store test_coordinator_service test_datanode_service system_test
    COMMENT "Running all tests"
    VERBATIM
)

add_custom_target(check_verbose
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --verbose
    DEPENDS test_metadata_store test_coordinator_service test_datanode_service test_integration
    COMMENT "Running all tests with verbose output"
    VERBATIM
)
