include(FetchContent)

# SYSTEM keeps Catch2's headers out of our -Werror warning set.
FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.7.1
  GIT_SHALLOW ON
  SYSTEM
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

add_executable(gb_unit
  unit/test_cartridge.cpp
  unit/test_scheduler.cpp
  unit/test_serialize.cpp
  unit/test_state.cpp
  unit/test_timer.cpp
)
target_link_libraries(gb_unit PRIVATE gbcore Catch2::Catch2WithMain)
gb_set_warnings(gb_unit)
gb_set_sanitizers(gb_unit)
catch_discover_tests(gb_unit)

add_executable(gb_acceptance
  acceptance/main.cpp
  acceptance/png.cpp
  acceptance/sha256.cpp
)
target_link_libraries(gb_acceptance PRIVATE gbcore)
gb_set_warnings(gb_acceptance)
gb_set_sanitizers(gb_acceptance)

# Exits 0 with a notice when the ROMs have not been fetched, so a fresh clone
# still reports green.
add_test(NAME acceptance COMMAND gb_acceptance
  --roms ${CMAKE_SOURCE_DIR}/tests/roms
  --golden ${CMAKE_SOURCE_DIR}/tests/golden
)
