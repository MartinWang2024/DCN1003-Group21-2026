# Add SQLite3 library from extern
add_library(sqlite3 STATIC
        sqlite3.c
)

target_include_directories(sqlite3
        PUBLIC
        .
)

# Disable SQLite3 shell and testing
target_compile_definitions(sqlite3 PRIVATE
        SQLITE_OMIT_LOAD_EXTENSION
        SQLITE_THREADSAFE=1
)