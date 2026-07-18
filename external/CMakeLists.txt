cmake_minimum_required(VERSION 3.19)
project(external LANGUAGES C CXX)
include(FetchContent)
# =========================================================
# Option: allow user to provide local source directories
# =========================================================
# set(OPEN62541_SOURCE_DIR "" CACHE PATH "Local open62541 source (optional)")
# set(REPLXX_SOURCE_DIR "" CACHE PATH "Local replxx source (optional)")

# =========================================================
# open62541
# =========================================================
set(OPEN62541_BUILD_SHARED ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS       ON CACHE BOOL "" FORCE)
set(UA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(UA_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(UA_ENABLE_AMALGAMATION OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
        open62541
        GIT_REPOSITORY https://github.com/open62541/open62541.git
        GIT_TAG v1.5.4
        GIT_SHALLOW ON
        GIT_PROGRESS ON
)
FetchContent_MakeAvailable(open62541)
# =========================================================
# replxx
# =========================================================
set(REPLXX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(REPLXX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
        replxx
        GIT_REPOSITORY https://github.com/AmokHuginnsson/replxx.git
        GIT_SHALLOW ON
        GIT_PROGRESS ON
    )

# =========================================================
# sdl
# =========================================================

FetchContent_Declare(
    sdl
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.10
    GIT_SHALLOW ON
    GIT_PROGRESS ON
)
# =========================================================
# imgui
# =========================================================
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.8
    GIT_SHALLOW ON
    GIT_PROGRESS ON
)

# =========================================================
# MakeAvailable
# =========================================================
set(CMAKE_CXX_FLAGS_ORIG "${CMAKE_CXX_FLAGS}")
set(CMAKE_C_FLAGS_ORIG "${CMAKE_C_FLAGS}")

if(MSVC)
  string(APPEND CMAKE_CXX_FLAGS " /W0")
  string(APPEND CMAKE_C_FLAGS " /W0")
else()
  string(APPEND CMAKE_CXX_FLAGS " -w")
  string(APPEND CMAKE_C_FLAGS " -w")
endif()

FetchContent_MakeAvailable(replxx sdl imgui)

# Restore original flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_ORIG}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_ORIG}")
# =========================================================
# sqlite3 library
# =========================================================
add_library(sqlite3 STATIC sqlite/sqlite3.c)
set_target_properties(sqlite3 PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_options(sqlite3 PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W0>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-w>
)
target_include_directories(sqlite3 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/sqlite
)

add_library(sqlite3_dll SHARED sqlite/sqlite3.c)
set_target_properties(sqlite3_dll PROPERTIES OUTPUT_NAME "sqlite3" POSITION_INDEPENDENT_CODE ON)
target_compile_options(sqlite3_dll PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W0>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-w>
)
target_include_directories(sqlite3_dll PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/sqlite
)

# =========================================================
# tinycthread library
# =========================================================
add_library(tinycthread STATIC tinycthread.c)
target_compile_options(tinycthread PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W0>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-w>
)
target_include_directories(tinycthread PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# =========================================================
# imgui
# =========================================================
set(IMGUI_SRCS
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

add_library(imgui STATIC ${IMGUI_SRCS})
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${sdl_SOURCE_DIR}/include
    ${Vulkan_INCLUDE_DIRS}
)
target_link_libraries(imgui PUBLIC SDL3-static ${Vulkan_LIBRARIES})

set(IMPLOT_SOURCE
    implot/implot.cpp
    implot/implot_demo.cpp
    implot/implot_items.cpp
)

add_library(implot STATIC ${IMPLOT_SOURCE})
target_include_directories(implot PUBLIC ${IMPLOT_SOURCE_DIR} ${imgui_SOURCE_DIR})
target_link_libraries(implot PUBLIC imgui)

add_library(implot::implot ALIAS implot)
target_include_directories(implot PUBLIC
    implot/
)

