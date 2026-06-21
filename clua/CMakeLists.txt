cmake_minimum_required(VERSION 3.11)
project(Clua C CXX)

# Set C standard to C11
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


find_package(Threads REQUIRED)
# Enable all warnings and sanitizers for GCC/Clang
if (WIN32)
  add_definitions(-D_WIN32)
endif()
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
  # Enforce static linking
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
endif()

# Source files
set(SOURCES
    src/main.c
    src/log.c
    src/debug_buf.c
    modules/mirua_module.c
    modules/mirua_to_string.c
    modules/mirua_serialization.c
    # modules/file/file_windows.c
    src/utils.c
)

# Executable
add_executable(main ${SOURCES})
target_compile_definitions(main PRIVATE LOG_USE_COLOR _CRT_SECURE_NO_WARNINGS _GNU_SOURCE)
target_include_directories(main PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/modules
)
target_link_libraries(main PRIVATE
    open62541
    replxx::replxx
    tinycthread
    sqlite3
    Threads::Threads

)

if (WIN32)
    target_link_libraries(main PRIVATE
        ws2_32
        iphlpapi
    )
endif()