cmake_minimum_required(VERSION 3.12)
project(future)

set(CMAKE_CXX_STANDARD 14)

include_directories(~/eos/contracts)

add_library(future future.cpp future.hpp)