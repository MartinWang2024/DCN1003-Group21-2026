add_library(
        openssl
        STATIC
        src/openssl.cpp
)

target_include_directories(
        openssl
        PUBLIC
        libopenssl/include

)

target_link_libraries(
        openssl
        PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/openssl/libopenssl/lib/libcrypto-3-x64.dll
        ${CMAKE_SOURCE_DIR}/third_party/openssl/libopenssl/lib/libssl-3-x64.dll
)

# 运行时依赖的 DLL - 使用自定义命令复制到输出目录
add_custom_target(copy_openssl_dlls ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/third_party/openssl/libopenssl/lib/libcrypto-3-x64.dll
        ${CMAKE_SOURCE_DIR}/third_party/openssl/libopenssl/lib/libssl-3-x64.dll
        ${CMAKE_BINARY_DIR}/third_party/openssl
    COMMENT "Copying OpenSSL DLLs to build directory"
)
add_dependencies(openssl copy_openssl_dlls)