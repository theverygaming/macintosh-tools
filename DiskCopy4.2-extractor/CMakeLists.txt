cmake_minimum_required(VERSION 3.10)
project(diskcopy-extract)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(diskcopy-extract "src/extract.cpp")
target_include_directories(diskcopy-extract PUBLIC "${PROJECT_SOURCE_DIR}/include")
