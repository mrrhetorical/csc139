cmake_minimum_required(VERSION 3.0.0)
project(hash C)

set(CMAKE_C_STANDARD 11)

add_executable(hash hashproj.c)

add_executable(sharedhash sharedhash.c)