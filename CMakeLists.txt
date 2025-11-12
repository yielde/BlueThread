cmake_minimum_required(VERSION 3.14)

project(BlueThread LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_executable(BlueThread
    test.cc
)

target_link_libraries(BlueThread PRIVATE Threads::Threads)


