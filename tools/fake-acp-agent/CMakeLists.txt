# Deterministic ACP agent used by the ACP process-integration test. Built when
# tests are on; the test locates it via the FAKE_ACP_AGENT_PATH compile define.
add_executable(fake-acp-agent main.cpp)
target_link_libraries(fake-acp-agent PRIVATE nlohmann_json::nlohmann_json)
target_compile_features(fake-acp-agent PRIVATE cxx_std_20)
maestro_set_warnings(fake-acp-agent)
