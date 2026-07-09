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

include(FetchContent)

# --- Third-party: nlohmann/json (CLI output parsing) ---
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
  GIT_SHALLOW    ON
)
FetchContent_MakeAvailable(nlohmann_json)

# --- Third-party: Catch2 (only when tests are on) ---
if(MAESTRO_BUILD_TESTS)
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
add_subdirectory(libs/providers)
add_subdirectory(libs/orchestrator)
add_subdirectory(libs/runtime)

# Console harness (real POSIX process driving). Not built on Windows until the
# cross-platform QtProcessBackend lands.
if(NOT WIN32)
  add_subdirectory(apps/maestro-cli)
endif()

if(MAESTRO_BUILD_TESTS)
  add_subdirectory(tests)
endif()
