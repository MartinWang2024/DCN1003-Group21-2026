add_executable(dcn_service
	src/service.cpp
)

target_include_directories(dcn_service
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/include
)

if (WIN32)
	target_link_libraries(dcn_service PRIVATE ws2_32)
endif ()

target_link_libraries(dcn_service
	PRIVATE
		openssl
		dcn_driver
)

add_custom_command(TARGET dcn_service POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		${CMAKE_SOURCE_DIR}/third_party/openssl/libopenssl/lib/libcrypto-3-x64.dll
		${CMAKE_SOURCE_DIR}/third_party/openssl/libopenssl/lib/libssl-3-x64.dll
		$<TARGET_FILE_DIR:dcn_service>

		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		${CMAKE_SOURCE_DIR}/key/app.key
		$<TARGET_FILE_DIR:dcn_service>
		COMMENT "Copying OpenSSL DLLs for dcn_service"
)