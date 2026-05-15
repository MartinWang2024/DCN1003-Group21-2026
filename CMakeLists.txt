cmake_minimum_required(VERSION 3.20)

project(DCN_2026 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(third_party)

add_subdirectory(Driver)

add_subdirectory(programs/Client)

add_subdirectory(programs/Server)

add_subdirectory(Tests)

# --- Server config ---
set(APP_VERSION "000001")
set(APP_TYPE "SERVER")
configure_file(config.h.in "${CMAKE_CURRENT_BINARY_DIR}/Service/config.h")

# --- Client config ---
set(APP_VERSION "000001")
set(APP_TYPE "programs/Client")
configure_file(config.h.in "${CMAKE_CURRENT_BINARY_DIR}/Client/config.h")

# --- Protocol config ---
set(APP_VERSION "000001")
set(APP_TYPE "PROTOCOL")
configure_file(config.h.in "${CMAKE_CURRENT_BINARY_DIR}/Driver/include/config.h")

# --- Targets and per-target include paths ---

target_include_directories(dcn_server PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/Server")

target_include_directories(dcn_client PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/Client")

target_include_directories(dcn_driver PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/Driver/include")