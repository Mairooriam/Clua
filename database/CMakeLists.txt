cmake_minimum_required(VERSION 3.11)
set(PROJECT_NAME database)
project(clua_dbualization C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

function(enable_strict target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
  else()
    target_compile_options(${target} PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Wshadow
      -Wconversion
      -Wsign-conversion
      -Wformat=2
      -Wundef
      -Wnull-dereference
      -Wdouble-promotion
      -Wcast-align
      $<$<COMPILE_LANGUAGE:CXX>:-Wold-style-cast>
      $<$<COMPILE_LANGUAGE:CXX>:-Wnon-virtual-dtor>
      $<$<COMPILE_LANGUAGE:CXX>:-Woverloaded-virtual>
    )
  endif()
endfunction()

function(enable_sanitizers target)
  if(ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT MSVC)
    target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE -fsanitize=address,undefined)
  endif()
endfunction()

find_package(Threads REQUIRED)

#Add tinycthread.c to sources
set(SOURCES
    # main.c
    db_access.c
    core/allocator.c
    log.c
)

add_library(clua_db SHARED ${SOURCES})
enable_strict(clua_db)
enable_sanitizers(clua_db)
target_compile_definitions(clua_db PRIVATE LOG_USE_COLOR) #_CRT_SECURE_NO_WARNINGS)
# target_compile_options(clua_db PRIVATE -g -fno-omit-frame-pointer)
target_include_directories(clua_db
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external
)

# target_link_options(clua_db PRIVATE -rdynamic)
target_link_libraries(clua_db PRIVATE sqlite3)

add_executable(clua_db_example main.c)
target_link_libraries(clua_db_example PUBLIC clua_db open62541::open62541 Threads::Threads)

# Keep compile_commands.json in repo root for LSP/tools
add_custom_target(sync_compile_commands ALL
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_BINARY_DIR}/compile_commands.json
          ${CMAKE_SOURCE_DIR}/compile_commands.json
  COMMENT "Copy compile_commands.json to source root"
  VERBATIM
)
add_dependencies(sync_compile_commands clua_db)




