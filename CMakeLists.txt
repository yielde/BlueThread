cmake_minimum_required(VERSION 3.14)

project(BlueThread LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set default build type to Debug if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

find_package(Threads REQUIRED)

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

# Add -rdynamic flag to export symbols for backtrace
set_target_properties(test_mutex PROPERTIES
    LINK_FLAGS "-rdynamic"
)

# Set include directories for all targets
include_directories(${CMAKE_SOURCE_DIR})

# Link Threads library to all targets
link_libraries(Threads::Threads)
