cmake_minimum_required(VERSION 3.9)
project(web_server_framework)

set(CMAKE_CXX_STANDARD 11)

add_executable(web_server_framework main.cpp tcp_server.h tcp_server.cpp)