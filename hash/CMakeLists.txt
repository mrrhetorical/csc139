cmake_minimum_required(VERSION 3.0.0)
project(hash C)

set(CMAKE_C_STANDARD 11)

add_executable(hash hashproj.c)

add_executable(sharedhash sharedhash.c)

add_executable(sharedhash_debug sharedhash.c)

target_compile_definitions(sharedhash_debug PRIVATE DEBUG)

add_executable(esharedhash esharedhash.c)

add_executable(esharedhash_debug esharedhash.c)

target_compile_definitions(esharedhash_debug PRIVATE DEBUG)