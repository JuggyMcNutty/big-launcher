set(SOURCES
  main.cpp
  core/app.cpp
  core/config.cpp
  core/layout_model.cpp
  platform/process.cpp
  platform/power.cpp
  platform/sysctl.cpp
  input/action_mapper.cpp
  ui/list_screen.cpp
  ui/anim.cpp
  ui/text_sprite.cpp
  ui/text_input.cpp
  ui/status_bar.cpp
  ui/shapes.cpp
  screens/home.cpp
  screens/settings/hub.cpp
  screens/settings/audio.cpp
  screens/settings/display.cpp
  screens/settings/network.cpp
  screens/settings/bluetooth.cpp
  screens/settings/system.cpp
  screens/settings/power.cpp
  screens/settings/launcher.cpp
  jobs/job_queue.cpp
  render/renderer_sdl.cpp
  render/image.cpp
  render/text.cpp
  render/sound.cpp
  render/util.cpp
  render/theme.cpp
)

if (BL_EVDEV AND LIBEVDEV_FOUND)
  list(APPEND SOURCES input/evdev_global.cpp)
endif ()
if (BL_CEC)
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
if (BL_EVDEV AND LIBEVDEV_FOUND)
  target_link_libraries(${EXECUTABLE_TITLE} PkgConfig::LIBEVDEV)
endif ()
if (BL_CEC)
  target_link_libraries(${EXECUTABLE_TITLE} PkgConfig::LIBCEC)
endif ()
