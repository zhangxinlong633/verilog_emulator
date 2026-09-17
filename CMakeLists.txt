cmake_minimum_required(VERSION 3.16)
project(vs C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(vs src/main.c)
target_include_directories(vs PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

enable_testing()
