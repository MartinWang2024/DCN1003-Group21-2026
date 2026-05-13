add_executable(dcn_client
        src/client.cpp
)

target_include_directories(dcn_client
    PRIVATE
        include
)

target_link_libraries(dcn_client
    PRIVATE
        dcn_driver
)

if (WIN32)
    target_link_libraries(dcn_client PRIVATE ws2_32)
endif ()

