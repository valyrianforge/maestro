# maestro_storage: SQLite persistence. Qt-free and orchestrator-free (pure
# repository), depends only on SQLiteCpp (which bundles sqlite3).
add_library(maestro_storage src/SqliteStore.cpp)
add_library(maestro::storage ALIAS maestro_storage)

target_include_directories(maestro_storage PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

target_link_libraries(maestro_storage
  PUBLIC maestro::core
  PRIVATE SQLiteCpp)

target_compile_features(maestro_storage PUBLIC cxx_std_20)
maestro_set_warnings(maestro_storage)
