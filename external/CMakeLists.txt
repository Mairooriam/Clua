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

# =========================================================
# Optional: make sure external libs don't install by default
# =========================================================
set_directory_properties(PROPERTIES EXCLUDE_FROM_ALL YES)
