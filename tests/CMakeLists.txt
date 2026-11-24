add_executable(bl_tests
  test_core.cpp
  ${PROJECT_SOURCE_DIR}/src/core/config.cpp
  ${PROJECT_SOURCE_DIR}/src/core/layout_model.cpp
  ${PROJECT_SOURCE_DIR}/src/input/action_mapper.cpp
  ${PROJECT_SOURCE_DIR}/src/platform/process.cpp
  ${PROJECT_SOURCE_DIR}/src/render/util.cpp
)
target_include_directories(bl_tests PRIVATE ${PROJECT_BINARY_DIR})
target_link_libraries(bl_tests
  PkgConfig::SDL3
  PkgConfig::LIBXML2
  PkgConfig::SPDLOG
  PkgConfig::FMT
)
add_test(NAME bl_tests COMMAND bl_tests)
