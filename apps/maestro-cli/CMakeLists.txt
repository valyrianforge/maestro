add_executable(maestro-cli main.cpp)
target_link_libraries(maestro-cli PRIVATE maestro::process maestro::providers maestro::core)
maestro_set_warnings(maestro-cli)
