cmake_minimum_required(VERSION 3.22.3)
project(TalEliaOs2)

set(CMAKE_CXX_STANDARD 11)

include_directories(.)

add_executable(TalEliaOs2 uthreads.h uthreads.cpp ex_2_tests_updated/tests/test3.in.cpp )
