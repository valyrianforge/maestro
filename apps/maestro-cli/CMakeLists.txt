add_executable(maestro-cli main.cpp)
target_link_libraries(maestro-cli PRIVATE
  maestro::runtime      # transitively: orchestrator, providers, process, core
  maestro::providers    # ClaudeProvider/NdjsonLineReader used directly here
  maestro::process)     # PosixProcessBackend/ProcessManager used directly here
maestro_set_warnings(maestro-cli)
