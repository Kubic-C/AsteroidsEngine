cmake_minimum_required(VERSION "3.12")
project(AsteroidsEngine)

set(BUILD_SHARED_LIBS true)
unset(BUILD_STATIC_LIBS)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/game")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/game/debug")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/game")

add_subdirectory(deps)
add_subdirectory(src)
add_subdirectory(src-test)