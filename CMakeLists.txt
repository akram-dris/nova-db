cmake_minimum_required(VERSION 3.10)
project(nova_db)

set(CMAKE_CXX_STANDARD 20)

add_executable(nova_db main.cpp parser.cpp)
