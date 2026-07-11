# maestro_acp: the Agent Client Protocol (ACP) client. Speaks the client half
# of ACP's JSON-RPC 2.0 wire format over a fakeable transport seam. Qt-free;
# nlohmann/json is part of its public API (ACP is inherently a JSON protocol).
add_library(maestro_acp
  src/JsonRpc.cpp
  src/AcpClient.cpp)
add_library(maestro::acp ALIAS maestro_acp)

target_include_directories(maestro_acp PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

target_link_libraries(maestro_acp
  PUBLIC maestro::core nlohmann_json::nlohmann_json)

target_compile_features(maestro_acp PUBLIC cxx_std_20)
maestro_set_warnings(maestro_acp)
