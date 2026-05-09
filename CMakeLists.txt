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