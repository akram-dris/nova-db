cmake_minimum_required(VERSION 3.10)
project(nova_db)

set(CMAKE_CXX_STANDARD 17)

add_executable(nova_db main.cpp parser.cpp pager.cpp serializer.cpp)
target_link_libraries(nova_db stdc++fs)
