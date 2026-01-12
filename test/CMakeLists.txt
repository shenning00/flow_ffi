cmake_minimum_required(VERSION 3.16)

# Find required packages for testing
# Note: flow-core is found in parent CMakeLists.txt, but we reference it here
# to ensure the imported target is available in this scope
if(NOT TARGET flow-core::flow-core)
    find_package(flow-core REQUIRED CONFIG)
    message(STATUS "test: Found flow-core package in test directory")
else()
    message(STATUS "test: flow-core::flow-core target already available from parent")
endif()

# Verify flow-core target properties for debugging
get_target_property(FLOW_CORE_LOCATION flow-core::flow-core IMPORTED_LOCATION_RELEASE)
if(NOT FLOW_CORE_LOCATION)
    get_target_property(FLOW_CORE_LOCATION flow-core::flow-core IMPORTED_LOCATION_NOCONFIG)
endif()
if(NOT FLOW_CORE_LOCATION)
    get_target_property(FLOW_CORE_LOCATION flow-core::flow-core IMPORTED_LOCATION)
endif()
message(STATUS "test: flow-core library location: ${FLOW_CORE_LOCATION}")

find_package(GTest QUIET)

if(NOT GTest_FOUND)
    # Download and build Google Test
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

# Create test executable
add_executable(flow_ffi_tests
    test_handle_manager.cpp
    test_error_handling.cpp
    test_basic_functionality.cpp
    test_env_factory.cpp
    test_module_phase6.cpp
    test_port_metadata.cpp
    test_port_metadata_free.cpp
)

# Link libraries
# Note: flow-core::flow-core must be listed explicitly even though flow_ffi
# links to it PUBLIC, because on Linux the test executable needs to resolve
# undefined symbols from libflow_ffi.so at link time.
target_link_libraries(flow_ffi_tests
    PRIVATE
        flow_ffi
        flow-core::flow-core
        gtest_main
        gtest
)

# Include directories
target_include_directories(flow_ffi_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# Add tests to CTest
include(GoogleTest)
gtest_discover_tests(flow_ffi_tests)