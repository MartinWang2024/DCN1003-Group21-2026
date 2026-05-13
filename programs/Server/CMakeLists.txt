add_executable(dcn_server
		src/server_main.cpp
		src/cmdreg.cpp
)

target_include_directories(dcn_server
	PRIVATE
		include
)

if (WIN32)
	target_link_libraries(dcn_server PRIVATE ws2_32)
endif ()

target_link_libraries(dcn_server
	PRIVATE
		protobuf
		openssl
		dcn_driver
)

add_custom_command(TARGET dcn_server POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		${CMAKE_SOURCE_DIR}/third_party/openssl/libopenssl/lib/libcrypto-3-x64.dll
		${CMAKE_SOURCE_DIR}/third_party/openssl/libopenssl/lib/libssl-3-x64.dll
		$<TARGET_FILE_DIR:dcn_server>

		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		${CMAKE_SOURCE_DIR}/key/app.key
		$<TARGET_FILE_DIR:dcn_server>
		COMMENT "Copying OpenSSL DLLs for dcn_server"
)