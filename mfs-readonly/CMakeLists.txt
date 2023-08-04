cmake_minimum_required(VERSION 3.10)
project(mfs-readonly)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(mfs-readonly "src/mfsro.cpp" "src/main.cpp")
target_include_directories(mfs-readonly PUBLIC "${PROJECT_SOURCE_DIR}/include")
