cmake_minimum_required(VERSION 3.18)
project(flow_ffi VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Platform-specific settings
if(WIN32)
    add_compile_definitions(FLOW_FFI_EXPORT=__declspec\(dllexport\))
else()
    add_compile_definitions(FLOW_FFI_EXPORT=__attribute__\(\(visibility\("default"\)\)\))
endif()

# Find flow-core package (includes nlohmann_json and other dependencies)
find_package(flow-core REQUIRED CONFIG)

message(STATUS "Found flow-core package")

option(FLOW_FFI_ENABLE_CUDA "Build CUDA image-path support" OFF)

# Propagate the gate down to flow_wire *before* the existing add_subdirectory
# call.  flow_wire reads its own option() after this CACHE BOOL … FORCE entry
# has been written, so it sees the value set here rather than its own default.
# Mirrors the FLOW_INTERWORK_WITH_OPENCV pattern in flow_wire/CMakeLists.txt.
set(FLOW_WIRE_WITH_CUDA ${FLOW_FFI_ENABLE_CUDA} CACHE BOOL
    "flow_wire: build CUDA-aware wire types and adapter nodes" FORCE)

# flow_wire: interwork layer that owns the canonical flow::Image wire type
# and the generic source/sink nodes that produce/consume it (ImageOpenNode,
# PreviewNode), plus optionally compiled cv::Mat adapter nodes.  Lives as a
# sibling directory; built as a static library and absorbed into flow_ffi.so.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../flow_wire ${CMAKE_CURRENT_BINARY_DIR}/flow_wire)

# Create shared library
add_library(flow_ffi SHARED
    src/flow_ffi.cpp
    src/handle_manager.cpp
    src/error_handling.cpp
    src/env_bridge.cpp
    src/factory_bridge.cpp
    # Phase 3-4 implementation
    src/graph_bridge.cpp
    src/node_bridge.cpp
    src/connection_bridge.cpp
    src/module_bridge.cpp
    src/type_conversions.cpp
    # Phase 5: Event System
    src/event_bridge.cpp
    # P10: Image primitives & built-in PreviewNode
    src/image_bridge.cpp
    # Phase D prep: texture registrar slot for CUDA/GPU plugin handoff
    src/texture_registrar_slot.cpp
    # Phase D: texture ops callbacks (plugin-extension pattern)
    src/texture_ops_slot.cpp
    # P12: non-CUDA GPU texture ops slot + host→texture bridge (always built)
    src/gpu_texture_ops_slot.cpp
    src/image_bridge_gpu.cpp
)

# Include directories
target_include_directories(flow_ffi
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link libraries (flow-core::flow-core brings in nlohmann_json transitively)
# PUBLIC linkage ensures that targets linking to flow_ffi will also link to flow-core,
# which is necessary to resolve undefined references in flow_ffi.so
target_link_libraries(flow_ffi
    PUBLIC
        flow-core::flow-core
        flow_wire
)

if (FLOW_FFI_ENABLE_CUDA)
    enable_language(CUDA)
    find_package(CUDAToolkit REQUIRED)
    target_compile_definitions(flow_ffi PUBLIC FLOW_FFI_HAS_CUDA=1)
    target_sources(flow_ffi PRIVATE src/image_bridge_cuda.cpp)
    target_link_libraries(flow_ffi PRIVATE CUDA::cudart CUDA::cuda_driver)

    # GL/EGL: required for cudaGraphicsGLRegisterImage.
    # We use libepoxy for function-pointer dispatch (same as Flutter/GTK on Linux)
    # and link the system OpenGL/EGL loader libs for cudaGL interop.
    find_package(OpenGL REQUIRED COMPONENTS OpenGL)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(EPOXY REQUIRED epoxy)
    target_link_libraries(flow_ffi PRIVATE OpenGL::GL ${EPOXY_LIBRARIES})
    target_include_directories(flow_ffi PRIVATE ${EPOXY_INCLUDE_DIRS})

    # CUDA GL interop header (cuda_gl_interop.h) lives under CUDAToolkit.
    # CUDAToolkit_INCLUDE_DIRS is set by find_package(CUDAToolkit).
    target_include_directories(flow_ffi PRIVATE ${CUDAToolkit_INCLUDE_DIRS})
else()
    target_compile_definitions(flow_ffi PUBLIC FLOW_FFI_HAS_CUDA=0)
    # Always provide the 4 texture FFI symbols even in the no-CUDA build so
    # that the test binary and any Dart FFI loader can resolve them (they
    # return FLOW_ERROR_NOT_IMPLEMENTED / no-op).
    target_sources(flow_ffi PRIVATE src/image_bridge_cuda_stub.cpp)
endif()

# Capability TU is compiled in BOTH modes; its body switches on FLOW_FFI_HAS_CUDA.
target_sources(flow_ffi PRIVATE src/cuda_capability.cpp)

# Platform-specific link options
if(APPLE)
    set_target_properties(flow_ffi PROPERTIES
        INSTALL_RPATH "@loader_path"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
elseif(UNIX)
    set_target_properties(flow_ffi PROPERTIES
        INSTALL_RPATH "$ORIGIN"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
endif()

# Installation
install(TARGETS flow_ffi
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES include/flow_ffi.h
    DESTINATION include
)

# Testing support
enable_testing()
add_subdirectory(test)
