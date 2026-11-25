cmake_minimum_required(VERSION 3.18)
project("foyer"
  LANGUAGES C CXX
  VERSION 2.1.0
)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(EXECUTABLE_TITLE "foyer")
add_compile_definitions("$<$<CONFIG:DEBUG>:DEBUG>")

# v2 options
option(FOYER_CEC "Build with HDMI-CEC support (libcec)" OFF)
option(FOYER_EVDEV "Build with global evdev input (Home/Vol)" ON)
option(FOYER_WERROR "Treat warnings as errors (CI builds)" OFF)

if (FOYER_WERROR)
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
else()
  add_compile_options(-Wall -Wextra)
endif ()

find_package(PkgConfig MODULE REQUIRED)
pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)
pkg_check_modules(SDL3_IMAGE REQUIRED IMPORTED_TARGET sdl3-image)
pkg_check_modules(SDL3_TTF REQUIRED IMPORTED_TARGET sdl3-ttf)
pkg_check_modules(LIBXML2 REQUIRED IMPORTED_TARGET libxml-2.0)
pkg_check_modules(FMT REQUIRED IMPORTED_TARGET fmt)
pkg_check_modules(SPDLOG REQUIRED IMPORTED_TARGET spdlog)

if (FOYER_EVDEV)
  pkg_check_modules(LIBEVDEV QUIET IMPORTED_TARGET libevdev)
  if (LIBEVDEV_FOUND)
    add_compile_definitions(FOYER_EVDEV)
  else()
    message(WARNING "libevdev not found; FOYER_EVDEV disabled")
  endif ()
endif ()

if (FOYER_CEC)
  pkg_check_modules(LIBCEC REQUIRED IMPORTED_TARGET libcec)
  add_compile_definitions(FOYER_CEC)
endif ()

configure_file(${PROJECT_SOURCE_DIR}/config/config.h.in ${PROJECT_BINARY_DIR}/lconfig.h)

add_subdirectory(src)

# Tests
enable_testing()
add_subdirectory(tests)
