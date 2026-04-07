cmake_minimum_required(VERSION 3.28)
project(ESet)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_executable(ESet
        test/1.cpp
        src.hpp)
