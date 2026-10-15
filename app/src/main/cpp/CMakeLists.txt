cmake_minimum_required(VERSION 3.22.1)
project(switch_runtime LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(switch_runtime SHARED runtime.cpp)
target_link_libraries(switch_runtime android log vulkan)
