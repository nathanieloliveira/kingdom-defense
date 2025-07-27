cmake_minimum_required(VERSION 3.31)
project(kingdom_defense C)

set(CMAKE_C_STANDARD 23)

find_package("SDL3")

add_executable(kingdom_defense main.c)

target_link_libraries(kingdom_defense SDL3::SDL3)
target_include_directories(kingdom_defense PRIVATE SDL3::SDL3)
