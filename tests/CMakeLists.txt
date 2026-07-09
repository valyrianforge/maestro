set(MAESTRO_TEST_SOURCES
  unit/test_smoke.cpp
  unit/test_process_manager.cpp
  unit/test_ndjson_reader.cpp
  unit/test_claude_provider.cpp
  unit/test_generic_provider.cpp
  unit/test_provider_contract.cpp
  unit/test_task_graph.cpp
  unit/test_agent_manager.cpp
  unit/test_workspace.cpp
  unit/test_orchestrator.cpp
  unit/test_result_collector.cpp)

if(NOT WIN32)
  # Integration tests exercise the real POSIX backend against system commands.
  list(APPEND MAESTRO_TEST_SOURCES
    integration/test_posix_backend.cpp
    integration/test_process_task_executor.cpp)
endif()

add_executable(maestro_unit_tests ${MAESTRO_TEST_SOURCES})

target_link_libraries(maestro_unit_tests PRIVATE
  Catch2::Catch2WithMain
  maestro::runtime) # transitively brings core/process/providers/orchestrator

catch_discover_tests(maestro_unit_tests)
