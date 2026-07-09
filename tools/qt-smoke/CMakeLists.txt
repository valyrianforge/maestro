# Tiny manual verifier for the Qt process backend (not part of ctest; needs a
# QCoreApplication). Built only under MAESTRO_WITH_QT.
add_executable(maestro-qt-smoke main.cpp)
target_link_libraries(maestro-qt-smoke PRIVATE
  Qt6::Core
  maestro::process_qt
  maestro::runtime)
maestro_set_warnings(maestro-qt-smoke)
