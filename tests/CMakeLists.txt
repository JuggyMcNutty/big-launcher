add_executable(foyer_tests
  test_core.cpp
  ${PROJECT_SOURCE_DIR}/src/core/config.cpp
  ${PROJECT_SOURCE_DIR}/src/core/layout_model.cpp
  ${PROJECT_SOURCE_DIR}/src/input/action_mapper.cpp
  ${PROJECT_SOURCE_DIR}/src/platform/process.cpp
  ${PROJECT_SOURCE_DIR}/src/platform/sysctl.cpp
  ${PROJECT_SOURCE_DIR}/src/render/util.cpp
  ${PROJECT_SOURCE_DIR}/src/ui/engine/units.cpp
)
target_include_directories(foyer_tests PRIVATE ${PROJECT_BINARY_DIR})
target_link_libraries(foyer_tests
  PkgConfig::SDL3
  PkgConfig::LIBXML2
  PkgConfig::SPDLOG
  PkgConfig::FMT
)
add_test(NAME foyer_tests COMMAND foyer_tests)
