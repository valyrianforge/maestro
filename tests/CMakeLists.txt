set(MAESTRO_TEST_SOURCES
  unit/test_smoke.cpp
  unit/test_jsonrpc.cpp
  unit/test_acp_client.cpp
  unit/test_approval_broker.cpp
  unit/test_steer_queue.cpp
  unit/test_session_controller.cpp
  unit/test_session_manager.cpp
  unit/test_nucleus.cpp
  unit/test_worktree_manager.cpp
  unit/test_process_manager.cpp
  unit/test_ndjson_reader.cpp
  unit/test_claude_provider.cpp
  unit/test_generic_provider.cpp
  unit/test_provider_contract.cpp
  unit/test_task_graph.cpp
  unit/test_agent_manager.cpp
  unit/test_workspace.cpp
  unit/test_orchestrator.cpp
  unit/test_scheduler.cpp
  unit/test_result_collector.cpp
  unit/test_sqlite_store.cpp
  unit/test_sqlite_store_v2.cpp
  integration/test_run_recorder.cpp)

if(NOT WIN32)
  # Integration tests exercise the real POSIX backend against system commands.
  list(APPEND MAESTRO_TEST_SOURCES
    integration/test_posix_backend.cpp
    integration/test_process_task_executor.cpp
    integration/test_worktree_integration.cpp
    integration/test_acp_process_integration.cpp)
endif()

add_executable(maestro_unit_tests ${MAESTRO_TEST_SOURCES})

target_link_libraries(maestro_unit_tests PRIVATE
  Catch2::Catch2WithMain
  maestro::acp
  maestro::worktree
  maestro::runtime) # transitively brings core/process/providers/orchestrator

# The ACP process-integration test spawns the deterministic fake agent; hand it
# the built binary's absolute path and ensure it is built first (POSIX only).
if(NOT WIN32)
  add_dependencies(maestro_unit_tests fake-acp-agent)
  target_compile_definitions(maestro_unit_tests PRIVATE
    FAKE_ACP_AGENT_PATH="$<TARGET_FILE:fake-acp-agent>")
endif()

catch_discover_tests(maestro_unit_tests)
