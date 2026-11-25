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
  ui/list_screen.cpp
  ui/engine/units.cpp
  ui/engine/node.cpp
  ui/engine/canvas.cpp
  ui/engine/text.cpp
  ui/engine/assets.cpp
  ui/engine/widgets.cpp
  ui/engine/focus.cpp
  ui/engine/scroller.cpp
  ui/engine/engine.cpp
  ui/anim.cpp
  ui/text_sprite.cpp
  ui/text_input.cpp
  ui/status_bar.cpp
  ui/shapes.cpp
  screens/home.cpp
  screens/quick_menu.cpp
  screens/settings/hub.cpp
  screens/settings/audio.cpp
  screens/settings/display.cpp
  screens/settings/network.cpp
  screens/settings/bluetooth.cpp
  screens/settings/system.cpp
  screens/settings/power.cpp
  screens/settings/launcher.cpp
  screens/settings/layout_editor.cpp
  jobs/job_queue.cpp
  render/renderer_sdl.cpp
  render/image.cpp
  render/text.cpp
  render/sound.cpp
  render/util.cpp
  render/theme.cpp
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
)
if (FOYER_EVDEV AND LIBEVDEV_FOUND)
  target_link_libraries(${EXECUTABLE_TITLE} PkgConfig::LIBEVDEV)
endif ()
if (FOYER_CEC)
  target_link_libraries(${EXECUTABLE_TITLE} PkgConfig::LIBCEC)
endif ()
