add_library(dcn_driver STATIC
    src/protocol.cpp
    src/database.cpp
    src/db_courses_handle.cpp
    src/db_administrators_handle.cpp
    src/SocketHandler.cpp
    src/log.cpp
    src/CmdHandler.cpp
    src/winsock_guard.cpp
)

target_include_directories(dcn_driver
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link against the sqlite3 library built in the main CMakeLists.txt
target_link_libraries(dcn_driver
    PRIVATE
        sqlite3
        protobuf
        openssl
)

if (WIN32)
    target_link_libraries(dcn_driver PUBLIC ws2_32)
endif ()
