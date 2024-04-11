cmake_minimum_required(VERSION 3.24)
project(Interval_lock)

set(CMAKE_CXX_STANDARD 20)

include_directories(include)

add_executable(Interval_lock src/main.cpp)