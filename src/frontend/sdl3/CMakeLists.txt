find_package(SDL3 CONFIG REQUIRED)

add_executable(gb_sdl3 main.cpp)
target_link_libraries(gb_sdl3 PRIVATE gbcore SDL3::SDL3)
gb_set_warnings(gb_sdl3)
gb_set_sanitizers(gb_sdl3)
