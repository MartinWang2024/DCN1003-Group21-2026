add_library(dcn_driver STATIC
    src/protocol.cpp
    src/database.cpp
)

target_include_directories(dcn_driver
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_SOURCE_DIR}/extern/sqlite3
)

# Link against the sqlite3 library built in the main CMakeLists.txt
target_link_libraries(dcn_driver
    PRIVATE
        sqlite3
        protobuf
)
