include(FetchContent)

# =========================================================
# Option: allow user to provide local source directories
# =========================================================
# set(OPEN62541_SOURCE_DIR "" CACHE PATH "Local open62541 source (optional)")
# set(REPLXX_SOURCE_DIR "" CACHE PATH "Local replxx source (optional)")

# =========================================================
# open62541
# =========================================================
if(OPEN62541_SOURCE_DIR)
  message(STATUS "Using local open62541: ${OPEN62541_SOURCE_DIR}")
  add_subdirectory(${OPEN62541_SOURCE_DIR} open62541-build)
else()
  message(STATUS "Fetching open62541 from GitHub")

  set(OPEN62541_BUILD_SHARED OFF CACHE BOOL "" FORCE)
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
endif()

# =========================================================
# replxx
# =========================================================
if(REPLXX_SOURCE_DIR)
  message(STATUS "Using local replxx: ${REPLXX_SOURCE_DIR}")
  add_subdirectory(${REPLXX_SOURCE_DIR} replxx-build)
else()
  message(STATUS "Fetching replxx from GitHub")

  set(REPLXX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(REPLXX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

  FetchContent_Declare(
        replxx
        GIT_REPOSITORY https://github.com/AmokHuginnsson/replxx.git
        GIT_SHALLOW ON
        GIT_PROGRESS ON
    )

  FetchContent_MakeAvailable(replxx)
endif()

# =========================================================
# sqlite3 library
# =========================================================
add_library(sqlite3 STATIC sqlite/sqlite3.c)
target_include_directories(sqlite3 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/sqlite
)

# =========================================================
# tinycthread library
# =========================================================
add_library(tinycthread STATIC tinycthread.c)
target_include_directories(tinycthread PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.8
    GIT_SHALLOW ON
    GIT_PROGRESS ON
)


FetchContent_MakeAvailable(imgui)

set(IMGUI_SRCS
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

FetchContent_Declare(
    sdl
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.10
    GIT_SHALLOW ON
    GIT_PROGRESS ON
)

FetchContent_MakeAvailable(sdl)

add_library(imgui STATIC ${IMGUI_SRCS})
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${sdl_SOURCE_DIR}/include
    ${Vulkan_INCLUDE_DIRS}
)
target_link_libraries(imgui PUBLIC SDL3-static ${Vulkan_LIBRARIES})

set(IMPLOT_SOURCE
    src/implot.cpp
    src/implot_demo.cpp
    src/implot_items.cpp
)
add_library(implot STATIC ${IMPLOT_SOURCE})
target_include_directories(implot PUBLIC ${IMPLOT_SOURCE_DIR} ${imgui_SOURCE_DIR})
target_link_libraries(implot PUBLIC imgui)

add_library(implot::implot ALIAS implot)
target_include_directories(implot PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(implot PUBLIC imgui)


# =========================================================
# Optional: make sure external libs don't install by default
# =========================================================
set_directory_properties(PROPERTIES EXCLUDE_FROM_ALL YES)
