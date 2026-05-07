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

