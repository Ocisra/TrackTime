cmake_minimum_required(VERSION 3.15)

project(yotta)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ./bin)

set(CMAKE_CXX_FLAGS "-pthread")
add_executable(yotta_daemon yotta_daemon.cpp timeTracking.cpp timeTracking.hpp socket.cpp socket.hpp util.cpp util.hpp)

add_executable(yotta yotta_cli.cpp util.cpp util.hpp)