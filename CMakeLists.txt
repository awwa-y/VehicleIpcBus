# ============================================================================
# VehicleIpcBus - CMake 构建（可选，面试加分）
# 最低要求 CMake 3.16（Qt6 需要 3.16+）
# ============================================================================
cmake_minimum_required(VERSION 3.16)
project(VehicleIpcBus LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 公共编译选项
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# ============================================================================
# 核心库 vipc (纯 C++17，不含 Qt)
# ============================================================================
add_library(vipc STATIC
    src/NodeClient.cpp
    src/MessageBus.cpp
)

target_include_directories(vipc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(vipc PUBLIC pthread)

# ============================================================================
# examples
# ============================================================================
add_subdirectory(examples/01_hello_rpc/client)
add_subdirectory(examples/01_hello_rpc/server)
add_subdirectory(examples/messagebus)

# Qt example 可选：没有 Qt6 就跳过，不影响核心库构建
find_package(Qt6 COMPONENTS Core Widgets QUIET)
if(Qt6_FOUND)
    message(STATUS "Qt6 found, building mock_hmi example")
    add_subdirectory(examples/mock_hmi)
else()
    message(STATUS "Qt6 not found, skip mock_hmi example (core library is OK)")
endif()
