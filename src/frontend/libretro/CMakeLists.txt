add_library(gb_libretro SHARED libretro.cpp)

# Cores are loaded by exact filename, so no "lib" prefix and no version suffix.
set_target_properties(gb_libretro PROPERTIES
  OUTPUT_NAME my_gb_libretro
  PREFIX ""
)

# The vendored header is third-party; SYSTEM keeps our warning set off it.
target_include_directories(gb_libretro SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gb_libretro PRIVATE gbcore)
gb_set_warnings(gb_libretro)
gb_set_sanitizers(gb_libretro)

# gbcore is static and lands inside the shared object, so it needs PIC too.
set_property(TARGET gb_libretro PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET gbcore PROPERTY POSITION_INDEPENDENT_CODE ON)
