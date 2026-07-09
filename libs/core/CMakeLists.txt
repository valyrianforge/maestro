# maestro_core: Qt-free interfaces + domain value types. Header-only for now.
add_library(maestro_core INTERFACE)
add_library(maestro::core ALIAS maestro_core)

target_include_directories(maestro_core INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

target_compile_features(maestro_core INTERFACE cxx_std_20)
