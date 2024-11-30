cmake_minimum_required(VERSION 3.11)
project(paracl)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-DDYNAMIC_ANALYZE -Wall -g -O0 -fsanitize=address -fsanitize=undefined")

enable_testing()

add_subdirectory(./src)
add_subdirectory(./tests)