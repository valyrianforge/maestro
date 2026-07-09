# maestro_orchestrator: pure domain — task graph, agents, workspace, and the
# concurrent Scheduler. Qt-free AND process-free; depends only on core
# interfaces so all scheduling/dependency/forwarding logic is testable with a
# fake executor.
find_package(Threads REQUIRED)

add_library(maestro_orchestrator
  src/TaskGraph.cpp
  src/AgentManager.cpp
  src/WorkspaceManager.cpp
  src/Scheduler.cpp)
add_library(maestro::orchestrator ALIAS maestro_orchestrator)

target_include_directories(maestro_orchestrator PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

target_link_libraries(maestro_orchestrator PUBLIC maestro::core Threads::Threads)
target_compile_features(maestro_orchestrator PUBLIC cxx_std_20)
maestro_set_warnings(maestro_orchestrator)
