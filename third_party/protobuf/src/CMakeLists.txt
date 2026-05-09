add_library(
        protobuf
        STATIC
        message.pb.cc
)

target_include_directories(
        protobuf
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
        protobuf
        PUBLIC
        libprotobuf)