cmake_minimum_required(VERSION 3.20)
project(RC)

set(CMAKE_CXX_STANDARD 20)

add_executable(RC Server.cpp Player.cpp file_manager.h file_manager.cpp)
