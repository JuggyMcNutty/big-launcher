add_executable(foyer_tests
  test_core.cpp
  test_engine.cpp
  test_model.cpp
  ${PROJECT_SOURCE_DIR}/src/core/model.cpp
  ${PROJECT_SOURCE_DIR}/src/core/config.cpp
  ${PROJECT_SOURCE_DIR}/src/core/layout_model.cpp
  ${PROJECT_SOURCE_DIR}/src/input/action_mapper.cpp
  ${PROJECT_SOURCE_DIR}/src/platform/process.cpp
  ${PROJECT_SOURCE_DIR}/src/platform/sysctl.cpp
  ${PROJECT_SOURCE_DIR}/src/render/util.cpp
  ${PROJECT_SOURCE_DIR}/src/ui/engine/units.cpp
  ${PROJECT_SOURCE_DIR}/src/ui/engine/node.cpp
  ${PROJECT_SOURCE_DIR}/src/ui/engine/canvas.cpp
  ${PROJECT_SOURCE_DIR}/src/ui/engine/text.cpp
  ${PROJECT_SOURCE_DIR}/src/ui/engine/assets.cpp
  ${PROJECT_SOURCE_DIR}/src/ui/engine/widgets.cpp
  ${PROJECT_SOURCE_DIR}/src/ui/engine/focus.cpp
  ${PROJECT_SOURCE_DIR}/src/ui/engine/scroller.cpp
  ${PROJECT_SOURCE_DIR}/src/ui/engine/engine.cpp
)
target_include_directories(foyer_tests PRIVATE ${PROJECT_BINARY_DIR})
target_compile_definitions(foyer_tests PRIVATE FOYER_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
if (LIBEVDEV_FOUND)
  target_sources(foyer_tests PRIVATE ${PROJECT_SOURCE_DIR}/src/input/airmouse.cpp)
  target_link_libraries(foyer_tests PkgConfig::LIBEVDEV)
endif ()
target_link_libraries(foyer_tests
  PkgConfig::SDL3
  PkgConfig::SDL3_IMAGE
  PkgConfig::SDL3_TTF
  PkgConfig::LIBXML2
  PkgConfig::SPDLOG
  PkgConfig::FMT
  Threads::Threads
)
add_test(NAME foyer_tests COMMAND foyer_tests)

target_compile_options(foyer_tests PRIVATE ${FOYER_WARNINGS})
