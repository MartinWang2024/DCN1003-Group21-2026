cmake_minimum_required(VERSION 3.20)

project(DCN_2026 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add SQLite3 library from extern
add_library(sqlite3 STATIC
    extern/sqlite3/sqlite3.c
)

target_include_directories(sqlite3
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/sqlite3
)

# Disable SQLite3 shell and testing
target_compile_definitions(sqlite3 PRIVATE SQLITE_OMIT_LOAD_EXTENSION)

add_subdirectory(Driver)

add_subdirectory(Client)

add_subdirectory(Service)

add_subdirectory(Tests)