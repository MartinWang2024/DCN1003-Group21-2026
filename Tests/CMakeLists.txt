add_executable(test_database
    test_database.cpp
)

target_link_libraries(test_database
    PRIVATE
        dcn_driver
        sqlite3
)

add_executable(test_database_file
    test_database_file.cpp
)

target_link_libraries(test_database_file
    PRIVATE
        dcn_driver
        sqlite3
)

enable_testing()
add_test(NAME test_database      COMMAND test_database
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME test_database_file COMMAND test_database_file
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
