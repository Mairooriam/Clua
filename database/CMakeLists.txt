cmake_minimum_required(VERSION 3.11)
set(PROJECT_NAME database)
project(clua_db C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

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
      -Wno-c++-compat
    #   $<$<COMPILE_LANGUAGE:CXX>:-Wold-style-cast>
    #   $<$<COMPILE_LANGUAGE:CXX>:-Wnon-virtual-dtor>
    #   $<$<COMPILE_LANGUAGE:CXX>:-Woverloaded-virtual>
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
find_package(core REQUIRED)
#Add tinycthread.c to sources
set(SOURCES
    # main.c
    db_access.c
main.c
lexer.c
parser.c
)

add_executable(clua_db_example ${SOURCES})
enable_strict(clua_db_example)
enable_sanitizers(clua_db_example)

target_link_libraries(clua_db_example PUBLIC core::core open62541::open62541 Threads::Threads)

# Keep compile_commands.json in repo root for LSP/tools
add_custom_target(sync_compile_commands ALL
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_BINARY_DIR}/compile_commands.json
          ${CMAKE_SOURCE_DIR}/compile_commands.json
  COMMENT "Copy compile_commands.json to source root"
  VERBATIM
)

# Copy schema to bin
# TODO: remove the schema txt file and have it in code?
add_custom_target(copy_schema ALL
  COMMAND ${CMAKE_COMMAND} -E copy
          ${CMAKE_SOURCE_DIR}/schema.txt
          ${CMAKE_BINARY_DIR}/data/schema.txt
  COMMENT "Copy schema.txt to bin dir"
  VERBATIM
)

add_custom_command(TARGET clua_db_example POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:sqlite3_dll>
    $<TARGET_FILE_DIR:clua_db_example>
)
add_dependencies(clua_db_example sqlite3_dll copy_schema)

add_dependencies(sync_compile_commands clua_db_example)




