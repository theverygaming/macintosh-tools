cmake_minimum_required(VERSION 3.10)
project(mfstools)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories("${PROJECT_SOURCE_DIR}/include")

add_executable(mfstools-dir "src/common.cpp" "src/dir.cpp")
