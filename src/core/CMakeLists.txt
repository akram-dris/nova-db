add_library(novadb_core STATIC
    index.cpp
    pager.cpp
    parser.cpp
    serializer.cpp
    thread_pool.cpp
    wal.cpp
)

target_include_directories(novadb_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
