set(SOURCES
  main.cpp
  core/app.cpp
  core/config.cpp
  core/layout_model.cpp
  core/model.cpp
  platform/process.cpp
  platform/power.cpp
  platform/sysctl.cpp
  input/action_mapper.cpp
  ui/engine/units.cpp
  ui/engine/node.cpp
  ui/engine/canvas.cpp
  ui/engine/text.cpp
  ui/engine/assets.cpp
  ui/engine/widgets.cpp
  ui/engine/focus.cpp
  ui/engine/scroller.cpp
  ui/engine/engine.cpp
  screens/kit.cpp
  screens/shell.cpp
  screens/settings.cpp
  screens/keyboard.cpp
  screens/editor.cpp
  screens/quick_panel.cpp
  jobs/job_queue.cpp
  render/sound.cpp
  render/util.cpp
)

if (FOYER_EVDEV AND LIBEVDEV_FOUND)
  list(APPEND SOURCES input/evdev_global.cpp)
endif ()
if (FOYER_CEC)
  list(APPEND SOURCES input/cec.cpp)
endif ()

add_library(inih external/ini.c)
target_compile_definitions(inih PRIVATE INI_MAX_LINE=1027)

add_executable(${EXECUTABLE_TITLE} ${SOURCES})
target_include_directories(${EXECUTABLE_TITLE} PRIVATE ${PROJECT_BINARY_DIR})
target_link_libraries(${EXECUTABLE_TITLE}
  PkgConfig::SDL3
  PkgConfig::SDL3_IMAGE
  PkgConfig::SDL3_TTF
  PkgConfig::LIBXML2
  PkgConfig::FMT
  PkgConfig::SPDLOG
  inih
  Threads::Threads
)
if (FOYER_EVDEV AND LIBEVDEV_FOUND)
  target_link_libraries(${EXECUTABLE_TITLE} PkgConfig::LIBEVDEV)
endif ()
if (FOYER_CEC)
  target_link_libraries(${EXECUTABLE_TITLE} PkgConfig::LIBCEC)
endif ()

install(TARGETS ${EXECUTABLE_TITLE} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY ${PROJECT_SOURCE_DIR}/data/ DESTINATION ${CMAKE_INSTALL_DATADIR}/foyer)

target_compile_options(${EXECUTABLE_TITLE} PRIVATE ${FOYER_WARNINGS})
