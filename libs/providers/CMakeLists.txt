# maestro_providers: concrete IProvider implementations. Depends on
# nlohmann/json for CLI output parsing (Qt-free).
add_library(maestro_providers
  src/NdjsonLineReader.cpp
  src/ClaudeProvider.cpp
  src/GenericCliProvider.cpp)
add_library(maestro::providers ALIAS maestro_providers)

target_include_directories(maestro_providers PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

target_link_libraries(maestro_providers
  PUBLIC maestro::core
  PRIVATE nlohmann_json::nlohmann_json)

target_compile_features(maestro_providers PUBLIC cxx_std_20)
maestro_set_warnings(maestro_providers)
