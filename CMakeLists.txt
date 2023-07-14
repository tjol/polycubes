cmake_minimum_required(VERSION 3.20)

project(polycubes CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT MSVC)
    add_compile_options(-Wall -Wextra -pedantic)
endif()

add_subdirectory(src)
