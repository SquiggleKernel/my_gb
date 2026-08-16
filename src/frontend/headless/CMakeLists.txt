add_executable(gb_headless main.cpp)
target_link_libraries(gb_headless PRIVATE gbcore)
gb_set_warnings(gb_headless)
gb_set_sanitizers(gb_headless)
