cmake_minimum_required(VERSION 3.14)

project(BlueThread LANGUAGES CXX)

# Generate compile_commands.json for clangd and other tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set default build type to Debug if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

find_package(Threads REQUIRED)

# Google Test - 使用项目下的 googletest submodule
if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/googletest/CMakeLists.txt")
  set(googletest_SOURCE_DIR "${CMAKE_SOURCE_DIR}/third_party/googletest")
  set(googletest_BINARY_DIR "${CMAKE_BINARY_DIR}/third_party/googletest")
  # For Windows: Prevent overriding the parent project's compiler/linker settings
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})
  message(STATUS "Using googletest from ${googletest_SOURCE_DIR}")
else()
  message(FATAL_ERROR "Google Test not found. Please ensure googletest submodule is initialized: git submodule update --init --recursive")
endif()

add_executable(test
    test.cc
)

add_executable(test_mutex
    qa/test_mutex.cc
    synchronization/mutex.cc
    synchronization/utils.cc
    synchronization/backtrace.cc
)

add_executable(test_cond
    qa/test_cond.cc
    synchronization/mutex.cc
    synchronization/utils.cc
    synchronization/backtrace.cc
    synchronization/condition_variable.cc
    thread/thread.cc
    thread/utils.cc
)

add_executable(test_threadpool
    qa/test_threadpool.cc
    thread/threadpool.cc
    thread/heartbeat.cc
    thread/thread.cc
    thread/utils.cc
    synchronization/mutex.cc
    synchronization/utils.cc
    synchronization/backtrace.cc
    synchronization/shared_mutex.cc
    synchronization/condition_variable.cc
)

# Link gtest libraries to test_threadpool
# Note: ceph-release-1.7.x uses gtest/gtest_main instead of GTest::gtest/GTest::gtest_main
target_link_libraries(test_threadpool
    PRIVATE
    gtest
    gtest_main
    Threads::Threads
)

# Add -rdynamic flag to export symbols for backtrace
set_target_properties(test_mutex PROPERTIES
    LINK_FLAGS "-rdynamic"
)

# Set include directories for all targets
include_directories(${CMAKE_SOURCE_DIR})

# Link Threads library to all targets
link_libraries(Threads::Threads)
