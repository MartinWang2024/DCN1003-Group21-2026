add_library(
        protobuf
        STATIC
        src/message.pb.cc
)

target_link_directories(
        protobuf
        PUBLIC
        src/..

)