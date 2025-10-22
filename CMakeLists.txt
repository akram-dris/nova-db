cmake_minimum_required(VERSION 3.10)
project(nova_db)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(src/core)

add_executable(nova_db src/cli/main.cpp)
target_include_directories(nova_db PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/core)
target_link_libraries(nova_db novadb_core stdc++fs)
