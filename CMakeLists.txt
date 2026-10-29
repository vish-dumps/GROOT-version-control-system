cmake_minimum_required(VERSION 3.20)
project(groot VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(groot_core
    src/repository.cpp
    src/index.cpp
    src/object_store.cpp
    src/sha256.cpp
    src/utils.cpp
)
target_include_directories(groot_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(groot src/main.cpp)
target_link_libraries(groot PRIVATE groot_core)

include(CTest)
if(BUILD_TESTING)
    add_executable(groot_tests tests/foundation_test.cpp tests/staging_test.cpp)
    target_link_libraries(groot_tests PRIVATE groot_core)
    target_include_directories(groot_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
    add_test(NAME groot_core COMMAND groot_tests)
endif()
