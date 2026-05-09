set(protobuf_BUILD_TESTS OFF CACHE BOOL "关闭测试" FORCE)
set(protobuf_BUILD_EXAMPLES OFF CACHE BOOL "关闭示例" FORCE)

add_subdirectory(library/cmake)
add_subdirectory(src)
include_directories(library/src)

