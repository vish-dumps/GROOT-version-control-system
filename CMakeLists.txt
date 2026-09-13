cmake_minimum_required(VERSION 4.3)

project(GROOT)

set(CMAKE_CXX_STANDARD 20)

add_executable(GROOT
        src/main.cpp
        src/Repository.cpp
)

target_include_directories(GROOT PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)