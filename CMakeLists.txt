cmake_minimum_required(VERSION 3.16)
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
