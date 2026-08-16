cmake_minimum_required(VERSION 3.25)
project(my_gb LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

option(GB_BUILD_SDL3     "Build the SDL3 frontend"     ON)
option(GB_BUILD_HEADLESS "Build the headless runner"   ON)
option(GB_BUILD_LIBRETRO "Build the libretro core"     OFF)
option(GB_BUILD_WASM     "Build the WebAssembly frontend" OFF)
option(GB_BUILD_DEBUGGER "Build the ImGui debugger"    OFF)
option(GB_BUILD_BENCH    "Build benchmarks"            OFF)
option(GB_BUILD_TESTS    "Build tests"                 ON)
option(GB_BUILD_FUZZ     "Build libFuzzer harnesses"   OFF)
option(GB_SANITIZE       "Enable ASan + UBSan"         OFF)
option(GB_WERROR         "Treat warnings as errors"    ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(Warnings)
include(Sanitizers)

add_subdirectory(src/core)

if(GB_BUILD_HEADLESS)
  add_subdirectory(src/frontend/headless)
endif()

if(GB_BUILD_SDL3)
  add_subdirectory(src/frontend/sdl3)
endif()

if(GB_BUILD_LIBRETRO)
  add_subdirectory(src/frontend/libretro)
endif()

if(GB_BUILD_WASM)
  add_subdirectory(src/frontend/wasm)
endif()

if(GB_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
