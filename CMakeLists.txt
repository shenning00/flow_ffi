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

# Absorbed wire layer (formerly the flow_wire submodule): owns the canonical
# flow::Image wire type and the generic source/sink nodes (ImageOpenNode,
# PreviewNode, DisplayValueNode), plus optionally compiled cv::Mat adapter
# nodes and CUDA-aware nodes.  Compiled directly into flow_ffi.so from
# src/wire/ and include/wire/ — registration entry point is src/wire/Register.cpp.

# cv::Mat <-> flow::Image adapter nodes, auto-disabled if OpenCV is not found.
option(FLOW_INTERWORK_WITH_OPENCV
       "Build cv::Mat <-> flow::Image adapter nodes (auto-disabled if OpenCV not found)"
       ON)
if(FLOW_INTERWORK_WITH_OPENCV)
    find_package(OpenCV CONFIG QUIET)
    if(OpenCV_FOUND)
        message(STATUS "flow_ffi: OpenCV ${OpenCV_VERSION} found — cv::Mat adapters enabled")
    else()
        message(STATUS "flow_ffi: OpenCV not found — cv::Mat adapters disabled")
        set(FLOW_INTERWORK_WITH_OPENCV OFF)
    endif()
endif()

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
    # Absorbed wire layer: generic node registration entry point
    src/wire/Register.cpp
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
)

# OpenCV adapter nodes (folded in from the former flow_wire library).
if(FLOW_INTERWORK_WITH_OPENCV)
    target_compile_definitions(flow_ffi PRIVATE FLOW_INTERWORK_WITH_OPENCV)
    target_include_directories(flow_ffi PRIVATE ${OpenCV_INCLUDE_DIRS})
    # Link OpenCV PUBLIC: any consumer calling RegisterAllNodes transitively
    # pulls these in (today only flow_ffi.so itself).
    target_link_libraries(flow_ffi PUBLIC ${OpenCV_LIBS})
endif()

if (FLOW_FFI_ENABLE_CUDA)
    enable_language(CUDA)
    find_package(CUDAToolkit REQUIRED)
    target_compile_definitions(flow_ffi PUBLIC FLOW_FFI_HAS_CUDA=1)
    # Compile the absorbed wire CUDA nodes (formerly gated by FLOW_WIRE_WITH_CUDA).
    target_compile_definitions(flow_ffi PRIVATE WIRE_WITH_CUDA)
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

# Public wire headers — the `#include <wire/...>` surface absorbed from the
# former flow_wire library (exposed via the PUBLIC include dir above).
install(DIRECTORY include/wire DESTINATION include)

# Testing support
enable_testing()
add_subdirectory(test)

# ---------------------------------------------------------------------------
# Code formatting (clang-format).  Globs src/ and include/ recursively, so the
# absorbed wire sources under src/wire/ and include/wire/ are covered too.
#
#   cmake --build build --target format        # rewrite files in place
#   cmake --build build --target format-check   # verify, non-zero on drift (CI)
# ---------------------------------------------------------------------------
find_program(CLANG_FORMAT_EXE NAMES clang-format clang-format-18 clang-format-17)
if(CLANG_FORMAT_EXE)
    file(GLOB_RECURSE FLOW_FFI_FORMAT_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test/*.hpp
    )
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i ${FLOW_FFI_FORMAT_SOURCES}
        COMMENT "clang-format -i over flow_ffi sources (${CLANG_FORMAT_EXE})"
        VERBATIM
    )
    add_custom_target(format-check
        COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror ${FLOW_FFI_FORMAT_SOURCES}
        COMMENT "clang-format compliance check (no files modified)"
        VERBATIM
    )
else()
    message(STATUS "clang-format not found — 'format'/'format-check' targets unavailable")
endif()
