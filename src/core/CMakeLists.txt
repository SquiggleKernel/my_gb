add_library(gbcore STATIC
  bus.cpp
  gb.cpp
  joypad.cpp
  scheduler.cpp
  serial.cpp
  timer.cpp
  apu/apu.cpp
  cart/cartridge.cpp
  cart/header.cpp
  cart/mbc.cpp
  cpu/disasm.cpp
  cpu/sm83.cpp
  ppu/ppu.cpp
)

target_include_directories(gbcore PUBLIC ${CMAKE_SOURCE_DIR}/src)
gb_set_warnings(gbcore)
gb_set_sanitizers(gbcore)
