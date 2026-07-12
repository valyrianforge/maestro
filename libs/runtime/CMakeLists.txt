# maestro_runtime: wiring that connects the pure orchestrator domain to real
# process execution. Platform-agnostic: ProcessTaskExecutor takes a backend
# factory, so callers pick PosixProcessBackend or QtProcessBackend.
add_library(maestro_runtime
  src/ProviderRegistry.cpp
  src/ResultCollector.cpp
  src/ProcessTaskExecutor.cpp
  src/RunRecorder.cpp)
add_library(maestro::runtime ALIAS maestro_runtime)

target_include_directories(maestro_runtime PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

target_link_libraries(maestro_runtime PUBLIC
  maestro::core
  maestro::acp
  maestro::orchestrator
  maestro::providers
  maestro::process
  maestro::storage)

target_compile_features(maestro_runtime PUBLIC cxx_std_20)
maestro_set_warnings(maestro_runtime)
