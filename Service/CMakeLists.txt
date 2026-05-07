add_executable(dcn_service
	src/service.cpp
)

target_link_libraries(dcn_service
	PRIVATE
		dcn_driver
)

if (WIN32)
	target_link_libraries(dcn_service PRIVATE ws2_32)
endif ()

