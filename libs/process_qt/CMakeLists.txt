# maestro_process_qt: cross-platform QProcess-based backend. Only built when
# Qt is available (MAESTRO_WITH_QT). Depends on Qt6::Core only.
add_library(maestro_process_qt src/QtProcessBackend.cpp)
add_library(maestro::process_qt ALIAS maestro_process_qt)

target_include_directories(maestro_process_qt PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

target_link_libraries(maestro_process_qt
  PUBLIC maestro::process
  PRIVATE Qt6::Core)

target_compile_features(maestro_process_qt PUBLIC cxx_std_20)
maestro_set_warnings(maestro_process_qt)
