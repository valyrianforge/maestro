set(MAESTRO_TEST_SOURCES
  unit/test_smoke.cpp
  unit/test_process_manager.cpp)

if(NOT WIN32)
  # Integration tests exercise the real POSIX backend against system commands.
  list(APPEND MAESTRO_TEST_SOURCES integration/test_posix_backend.cpp)
endif()

add_executable(maestro_unit_tests ${MAESTRO_TEST_SOURCES})

target_link_libraries(maestro_unit_tests PRIVATE
  Catch2::Catch2WithMain
  maestro::core
  maestro::process)

catch_discover_tests(maestro_unit_tests)
