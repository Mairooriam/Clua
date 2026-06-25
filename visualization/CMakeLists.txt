cmake_minimum_required(VERSION 3.11)
set(PROJECT_NAME visualization)
project(Visualization C CXX)

# Set C standard to C11
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_definitions(_AMD64_)
# Enable all warnings and sanitizers for GCC/Clang
if(MSVC)
  message(STATUS "MSVC compiler selected: using /W4 warnings")
  add_compile_options(/W4)
  add_compile_options(/external:W0)
else()
  message(STATUS "Using GCC or clang compiler ")
  add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -fdiagnostics-color=always
        -Wno-newline-eof
    )
endif()

find_package(Threads REQUIRED)
find_package(Vulkan REQUIRED)



#Add tinycthread.c to sources
set(SOURCES
    src/main.cpp
    src/plot.cpp
    src/OpcUa.cpp
    src/db_access.cpp
    src/db_render.cpp
    src/log.c
    src/imgui_SDL3_Vulkan.cpp
)

add_executable(Vis ${SOURCES})
target_compile_definitions(Vis PRIVATE LOG_USE_COLOR _CRT_SECURE_NO_WARNINGS)
target_compile_options(Vis PRIVATE -g -fno-omit-frame-pointer)
target_include_directories(Vis PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${implot_SOURCE_DIR}
    ${sdl_SOURCE_DIR}/include
    ${Vulkan_INCLUDE_DIRS}
)

target_link_options(Vis PRIVATE -rdynamic)
target_link_libraries(Vis PRIVATE SDL3-static ${Vulkan_LIBRARIES} imgui implot sqlite3)

