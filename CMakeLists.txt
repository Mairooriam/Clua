cmake_minimum_required(VERSION 3.19)
project(clua LANGUAGES C CXX)

add_compile_definitions(_WIN32)
add_subdirectory(clua)
add_subdirectory(playground)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)