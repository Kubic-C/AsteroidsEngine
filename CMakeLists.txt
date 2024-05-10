cmake_minimum_required(VERSION "3.12")

project(AsteroidsEngine)

message(STATUS "VCPKG: ${CMAKE_TOOLCHAIN_FILE}")

set(BUILD_STATIC_LIBS true)
unset(BUILD_SHARED_LIBS)

add_subdirectory(deps)
add_subdirectory(src)
