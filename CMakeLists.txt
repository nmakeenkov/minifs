cmake_minimum_required(VERSION 3.2)
project(minifs C)

set(CMAKE_C_STANDARD 99)

add_definitions(-Wall -Wextra -Werror -O2)

add_executable(minifs main.c file_storage.c)
