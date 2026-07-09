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

# --- Third-party: SQLiteCpp (bundles sqlite3; cross-platform persistence) ---
set(SQLITECPP_RUN_CPPLINT OFF CACHE BOOL "" FORCE)
set(SQLITECPP_RUN_CPPCHECK OFF CACHE BOOL "" FORCE)
set(SQLITECPP_INTERNAL_SQLITE ON CACHE BOOL "" FORCE)
FetchContent_Declare(
  SQLiteCpp
  GIT_REPOSITORY https://github.com/SRombauts/SQLiteCpp.git
  GIT_TAG        3.3.2
  GIT_SHALLOW    ON
)
FetchContent_MakeAvailable(SQLiteCpp)

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
add_subdirectory(libs/storage)
add_subdirectory(libs/runtime)

# Console harness (real POSIX process driving). Not built on Windows until the
# cross-platform QtProcessBackend lands.
if(NOT WIN32)
  add_subdirectory(apps/maestro-cli)
endif()

# Qt6 desktop GUI (opt-in).
if(MAESTRO_WITH_QT)
  # Qt 6.8's FindWrapOpenGL falls back to a literal "-framework AGL" when AGL is
  # absent. AGL was removed from modern macOS SDKs, breaking the link. Nothing
  # actually calls AGL, so we generate an empty stub AGL.framework and put it on
  # the framework search path. macOS-only; other platforms are untouched.
  if(APPLE)
    set(_agl_stub_dir "${CMAKE_BINARY_DIR}/agl-stub")
    if(NOT EXISTS "${_agl_stub_dir}/AGL.framework/AGL")
      file(MAKE_DIRECTORY "${_agl_stub_dir}/AGL.framework")
      execute_process(
        COMMAND ${CMAKE_CXX_COMPILER} -dynamiclib -x c /dev/null
                -install_name @rpath/AGL.framework/AGL
                -o "${_agl_stub_dir}/AGL.framework/AGL"
        RESULT_VARIABLE _agl_rc)
      if(NOT _agl_rc EQUAL 0)
        message(WARNING "Failed to build AGL stub; Qt link may fail.")
      endif()
    endif()
    unset(WrapOpenGL_AGL CACHE) # force re-resolution against the stub
    list(PREPEND CMAKE_FRAMEWORK_PATH "${_agl_stub_dir}")
  endif()

  find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
  add_subdirectory(libs/process_qt)
  add_subdirectory(apps/maestro-desktop)
  add_subdirectory(tools/qt-smoke)
endif()

if(MAESTRO_BUILD_TESTS)
  add_subdirectory(tests)
endif()

# --- Packaging: produce a DMG on macOS, an NSIS installer on Windows ---
if(MAESTRO_WITH_QT)
  set(CPACK_PACKAGE_NAME "Maestro")
  set(CPACK_PACKAGE_VENDOR "Maestro")
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "AI orchestration platform for local coding CLIs")
  set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
  set(CPACK_PACKAGE_INSTALL_DIRECTORY "Maestro")

  if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "Maestro")
  elseif(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_PACKAGE_NAME "Maestro")
    set(CPACK_NSIS_MODIFY_PATH ON)
    set(CPACK_PACKAGE_EXECUTABLES "maestro-desktop" "Maestro")
  else()
    set(CPACK_GENERATOR "TGZ")
  endif()

  include(CPack)
endif()
