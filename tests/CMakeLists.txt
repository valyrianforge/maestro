add_executable(maestro_unit_tests
  unit/test_smoke.cpp
  unit/test_process_manager.cpp)

target_link_libraries(maestro_unit_tests PRIVATE
  Catch2::Catch2WithMain
  maestro::core
  maestro::process)

catch_discover_tests(maestro_unit_tests)
