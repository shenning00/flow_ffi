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
    test_node_identity_registry.cpp
    test_node_lifecycle.cpp
    test_env_add_task_set_input_data.cpp
    test_p10_image_primitives.cpp
    test_image_open_node.cpp
    # Phase D: CUDA/Flutter texture interop API surface tests
    test_cuda_texture_interop.cpp
    # P12: non-CUDA GPU texture path (always built, no CUDA gate)
    test_flutter_texture_sink.cpp
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

# Phase D: CUDA + GL/EGL test dependencies.
# The interop test (test_cuda_texture_interop.cpp) uses EGL for headless GL
# context creation and cudaGraphicsGLRegisterImage directly in the GPU test.
if(FLOW_FFI_ENABLE_CUDA)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(EPOXY REQUIRED epoxy)
    target_link_libraries(flow_ffi_tests
        PRIVATE
            CUDA::cudart
            ${EPOXY_LIBRARIES}
    )
    target_include_directories(flow_ffi_tests PRIVATE ${EPOXY_INCLUDE_DIRS})
endif()

# Include directories
target_include_directories(flow_ffi_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# Add tests to CTest
include(GoogleTest)
gtest_discover_tests(flow_ffi_tests)

# --------------------------------------------------------------------------
# P13 capability probe — compiled in both CUDA and no-CUDA configurations.
# When CUDA is absent the probe should report available=0 count=0.
# When CUDA is present it should report available=1 count>=1 with a device name.
# --------------------------------------------------------------------------
add_executable(flow_ffi_cuda_probe test_cuda_capability_probe.cpp)
target_link_libraries(flow_ffi_cuda_probe
    PRIVATE
        flow_ffi
        flow-core::flow-core
)
target_include_directories(flow_ffi_cuda_probe
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# The probe exits 0 when the symbol is callable (regardless of whether CUDA
# hardware is present). A non-zero exit signals a missing symbol or crash.
add_test(NAME cuda_capability_probe COMMAND flow_ffi_cuda_probe)
set_tests_properties(cuda_capability_probe PROPERTIES
    PASS_REGULAR_EXPRESSION "available="
)

# --------------------------------------------------------------------------
# Phase G: CudaToImage adapter node bridge test — CUDA builds only.
# Allocates a 16x16 RGBA8 device buffer, fills it with 0xAB via cudaMemset,
# constructs a CudaToImage node, drives Compute, and verifies all output bytes
# equal 0xAB (i.e. cudaMemcpy2D worked correctly).
# --------------------------------------------------------------------------
if(FLOW_FFI_ENABLE_CUDA)
    add_executable(flow_ffi_cuda_to_image_bridge
        test_cuda_to_image_bridge.cpp
    )
    target_link_libraries(flow_ffi_cuda_to_image_bridge
        PRIVATE
            flow_ffi
            flow_wire
            flow-core::flow-core
            CUDA::cudart
    )
    target_include_directories(flow_ffi_cuda_to_image_bridge
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    add_test(NAME cuda_to_image_bridge COMMAND flow_ffi_cuda_to_image_bridge)
    set_tests_properties(cuda_to_image_bridge PROPERTIES
        PASS_REGULAR_EXPRESSION "PASS:"
    )

    # --------------------------------------------------------------------------
    # Phase E: FlowFlutterCudaPreview sink node tests
    #
    # Covers: class registration, construction+port layout, compute-with-no-input
    # no-op, and compute-with-mock-ops lifecycle (create/write/signal/destroy).
    # The mock-ops compute test follows the same A13 pattern as Phase D's
    # cuda_texture_interop suite: accepts both FLOW_ERROR_UNKNOWN (headless, no
    # GL context) and FLOW_SUCCESS (GL available) since the full register path
    # invokes cudaGraphicsGLRegisterImage with a fake GL texture name.
    # --------------------------------------------------------------------------
    add_executable(flow_ffi_flutter_cuda_preview
        test_flow_flutter_cuda_preview.cpp
    )
    target_link_libraries(flow_ffi_flutter_cuda_preview
        PRIVATE
            flow_ffi
            flow_wire
            flow-core::flow-core
            CUDA::cudart
            gtest_main
            gtest
    )
    target_include_directories(flow_ffi_flutter_cuda_preview
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    include(GoogleTest)
    gtest_discover_tests(flow_ffi_flutter_cuda_preview
        TEST_PREFIX "FlowFlutterCudaPreview."
    )
endif()