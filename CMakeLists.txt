cmake_minimum_required(VERSION "3.12")

if(WIN32)
    message("Using the VCPKG Toolchain for Windows")
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
endif()

project(AsteroidsEngine)

if(WIN32)
    message(STATUS "VCPKG: ${CMAKE_TOOLCHAIN_FILE}")
endif()

set(BUILD_STATIC_LIBS true)
unset(BUILD_SHARED_LIBS)

add_subdirectory(deps)
add_subdirectory(src)
