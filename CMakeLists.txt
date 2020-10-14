cmake_minimum_required(VERSION 3.17)
project(lab1 C)

set(CMAKE_C_STANDARD 11)

add_executable(lab1 main.c IO.h Constants.h IO.c)
target_link_libraries(lab1 pthread)