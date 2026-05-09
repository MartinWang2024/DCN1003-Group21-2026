cmake_minimum_required(VERSION 3.20)

project(DCN_2026 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(third_party)

add_subdirectory(Driver)

add_subdirectory(Client)

add_subdirectory(Service)

add_subdirectory(Tests)

# --- 服务端配置 ---
set(APP_VERSION "000001")
set(APP_TYPE "SERVER")
configure_file(config.h.in "${CMAKE_CURRENT_BINARY_DIR}/Service/config.h")

# --- 客户端配置 ---
set(APP_VERSION "000001")
set(APP_TYPE "CLIENT")
configure_file(config.h.in "${CMAKE_CURRENT_BINARY_DIR}/Client/config.h")

# --- 客户端配置 ---
set(APP_VERSION "000001")
set(APP_TYPE "PROTOCOL")
configure_file(config.h.in "${CMAKE_CURRENT_BINARY_DIR}/Driver/include/config.h")

# --- 定义目标并关联对应的头文件 ---

# 服务端目标
target_include_directories(dcn_service PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/Service")

# 客户端目标
target_include_directories(dcn_client PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/Client")

# 协议目标
target_include_directories(dcn_driver PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/Driver/include")