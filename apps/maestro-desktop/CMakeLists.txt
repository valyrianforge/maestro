# maestro-desktop: the Qt6 GUI. Only configured when MAESTRO_WITH_QT is ON and
# Qt6 is found (see top-level CMakeLists).
qt_standard_project_setup()

set(MAESTRO_DESKTOP_SOURCES
  main.cpp
  MainWindow.cpp
  EngineController.cpp)

qt_add_executable(maestro-desktop MACOSX_BUNDLE ${MAESTRO_DESKTOP_SOURCES})

target_link_libraries(maestro-desktop PRIVATE
  Qt6::Widgets
  maestro::runtime) # transitively: orchestrator, providers, process, core

set_target_properties(maestro-desktop PROPERTIES
  MACOSX_BUNDLE_BUNDLE_NAME "Maestro"
  MACOSX_BUNDLE_GUI_IDENTIFIER "com.maestro.desktop"
  MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
  MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
  WIN32_EXECUTABLE ON)

maestro_set_warnings(maestro-desktop)

# Drop load commands for frameworks we never actually call (notably the AGL
# stub pulled in transitively by Qt's WrapOpenGL). Keeps the bundle relocatable.
if(APPLE)
  target_link_options(maestro-desktop PRIVATE -Wl,-dead_strip_dylibs)
endif()

# --- Install + packaging (CPack) ---
install(TARGETS maestro-desktop
  BUNDLE  DESTINATION .
  RUNTIME DESTINATION bin)

# Bundle Qt runtime into the installed app so it is self-contained.
qt_generate_deploy_app_script(
  TARGET maestro-desktop
  OUTPUT_SCRIPT deploy_script
  NO_UNSUPPORTED_PLATFORM_ERROR)
install(SCRIPT ${deploy_script})
