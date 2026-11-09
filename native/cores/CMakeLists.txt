cmake_minimum_required(VERSION 3.14)
project(RetroUTestCore CXX ASM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(test_core test_core.cpp)
target_include_directories(test_core PRIVATE .)

wut_create_rpl(test_core)
wut_add_exports(test_core test_core.def)
