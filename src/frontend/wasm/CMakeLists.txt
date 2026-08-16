if(NOT EMSCRIPTEN)
  message(FATAL_ERROR "GB_BUILD_WASM requires configuring through emcmake")
endif()

add_executable(gb_wasm main.cpp)
target_link_libraries(gb_wasm PRIVATE gbcore)
gb_set_warnings(gb_wasm)
# EM_ASM takes its arguments as $0, $1 ... which -Wpedantic rejects.
target_compile_options(gb_wasm PRIVATE -Wno-dollar-in-identifier-extension)

set(GB_WASM_SHELL ${CMAKE_CURRENT_SOURCE_DIR}/shell.html)

set_target_properties(gb_wasm PROPERTIES
  OUTPUT_NAME index
  SUFFIX ".html"
  LINK_DEPENDS ${GB_WASM_SHELL}
)

target_link_options(gb_wasm PRIVATE
  "--shell-file=${GB_WASM_SHELL}"
  "-sENVIRONMENT=web"
  "-sALLOW_MEMORY_GROWTH=1"
  "-sEXPORTED_FUNCTIONS=['_main','_malloc','_free','_gb_load_rom','_gb_reset']"
  "-sEXPORTED_RUNTIME_METHODS=['HEAPU8','UTF8ToString']"
)
