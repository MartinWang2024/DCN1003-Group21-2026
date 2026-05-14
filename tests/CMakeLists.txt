include_directories(include)

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

add_executable(test_openssl
        test_openssl.cpp
)
target_link_libraries(test_openssl
    PRIVATE
        openssl
)

# 复制 OpenSSL DLL 到测试输出目录
add_custom_command(TARGET test_openssl POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/third_party/openssl/libopenssl/lib/libcrypto-3-x64.dll
        ${CMAKE_SOURCE_DIR}/third_party/openssl/libopenssl/lib/libssl-3-x64.dll
        $<TARGET_FILE_DIR:test_openssl>

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/key/app.key
        $<TARGET_FILE_DIR:test_openssl>
    COMMENT "Copying OpenSSL DLLs for test_openssl"
)

add_executable(test_protobuf
        test_protobuf.cpp
)
target_link_libraries(test_protobuf
    PRIVATE
        protobuf
)

add_executable(test_log
        test_log.cpp
)
target_link_libraries(test_log
    PRIVATE
        dcn_driver
)

add_executable(test_protocol
        test_protocol.cpp
)
target_link_libraries(test_protocol
    PRIVATE
        dcn_driver
        openssl
        protobuf
        Ws2_32
)

add_custom_command(TARGET test_protocol POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/third_party/openssl/libopenssl/lib/libcrypto-3-x64.dll
        ${CMAKE_SOURCE_DIR}/third_party/openssl/libopenssl/lib/libssl-3-x64.dll
        $<TARGET_FILE_DIR:test_protocol>

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/key/app.key
        $<TARGET_FILE_DIR:test_protocol>
    COMMENT "Copying OpenSSL DLLs and app.key for test_protocol"
)

enable_testing()
add_test(NAME test_database      COMMAND test_database
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME test_database_file COMMAND test_database_file
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME test_protocol      COMMAND test_protocol)

add_executable(test_cmdreg
        test_cmdreg.cpp
        ${CMAKE_SOURCE_DIR}/programs/Server/src/cmdreg.cpp
)
target_include_directories(test_cmdreg
    PRIVATE
        ${CMAKE_SOURCE_DIR}/programs/Server/include
)
target_link_libraries(test_cmdreg
    PRIVATE
        dcn_driver
        sqlite3
        protobuf
        openssl
)
if (WIN32)
    target_link_libraries(test_cmdreg PRIVATE Ws2_32)
endif ()

add_custom_command(TARGET test_cmdreg POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/third_party/openssl/libopenssl/lib/libcrypto-3-x64.dll
        ${CMAKE_SOURCE_DIR}/third_party/openssl/libopenssl/lib/libssl-3-x64.dll
        $<TARGET_FILE_DIR:test_cmdreg>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/key/app.key
        $<TARGET_FILE_DIR:test_cmdreg>
    COMMENT "Copying OpenSSL DLLs and app.key for test_cmdreg"
)

add_test(NAME test_cmdreg COMMAND test_cmdreg)

add_executable(test_socket_handler
        test_socket_handler.cpp
)
target_link_libraries(test_socket_handler
    PRIVATE
        dcn_driver
)
if (WIN32)
    target_link_libraries(test_socket_handler PRIVATE Ws2_32)
endif ()
add_test(NAME test_socket_handler COMMAND test_socket_handler)

