# maestro_runtime: wiring that connects the pure orchestrator domain to real
# process execution (providers + process layer). POSIX for now (uses
# PosixProcessBackend); QtProcessBackend will make it cross-platform later.
set(MAESTRO_RUNTIME_SOURCES
  src/ProviderRegistry.cpp
  src/ResultCollector.cpp)

if(NOT WIN32)
  list(APPEND MAESTRO_RUNTIME_SOURCES src/ProcessTaskExecutor.cpp)
endif()

add_library(maestro_runtime ${MAESTRO_RUNTIME_SOURCES})
add_library(maestro::runtime ALIAS maestro_runtime)

target_include_directories(maestro_runtime PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

target_link_libraries(maestro_runtime PUBLIC
  maestro::core
  maestro::orchestrator
  maestro::providers
  maestro::process)

target_compile_features(maestro_runtime PUBLIC cxx_std_20)
maestro_set_warnings(maestro_runtime)
