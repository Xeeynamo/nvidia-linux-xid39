cmake_minimum_required(VERSION 3.10..3.31)
project(poc C)

set(CMAKE_C_STANDARD 11)

find_package(SDL3 REQUIRED)
add_executable(${PROJECT_NAME} poc.c)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3)
