cmake_minimum_required(VERSION 3.24)
project(maestro LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

# Export compile_commands.json for tooling / clangd.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MAESTRO_WITH_QT "Build the Qt-backed process backend and UI" OFF)
option(MAESTRO_BUILD_TESTS "Build unit tests" ON)

include(cmake/Warnings.cmake)

# --- Third-party: Catch2 (only when tests are on) ---
if(MAESTRO_BUILD_TESTS)
  include(FetchContent)
  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    GIT_SHALLOW    ON
  )
  FetchContent_MakeAvailable(Catch2)
  list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
  include(CTest)
  include(Catch)
endif()

add_subdirectory(libs/core)
add_subdirectory(libs/process)

if(MAESTRO_BUILD_TESTS)
  add_subdirectory(tests)
endif()
