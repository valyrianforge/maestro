# maestro_process: process lifecycle spine. Qt-free by default; the optional
# QtProcessBackend (guarded by MAESTRO_WITH_QT) is added later.
set(MAESTRO_PROCESS_SOURCES
  src/ProcessManager.cpp
  src/FakeProcessBackend.cpp)

if(NOT WIN32)
  # Real POSIX backend (fork/exec/pipe/poll). Cross-platform QtProcessBackend
  # will supersede this once Qt is wired in.
  list(APPEND MAESTRO_PROCESS_SOURCES src/PosixProcessBackend.cpp)
endif()

add_library(maestro_process ${MAESTRO_PROCESS_SOURCES})
add_library(maestro::process ALIAS maestro_process)

target_include_directories(maestro_process PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

target_link_libraries(maestro_process PUBLIC maestro::core)
target_compile_features(maestro_process PUBLIC cxx_std_20)
maestro_set_warnings(maestro_process)
