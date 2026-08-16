#pragma once

#include <span>
#include <string>
#include <vector>

#include "core/bus.h"
#include "core/cpu/sm83.h"
#include "core/types.h"

namespace gb {

class Gb {
  public:
    Gb();

    bool load_rom(std::vector<u8> rom, std::string* err);
    void reset();

    void step_instruction();
    void run_cycles(u64 tcycles);
    // Runs until the PPU completes a frame, or a cycle cap if the LCD is off.
    void run_frame();

    const std::array<u8, kFramebufferPixels>& framebuffer() const {
        return bus_.ppu().framebuffer();
    }

    void set_buttons(u8 pressed) { bus_.joypad().set_buttons(pressed); }
    void set_serial_sink(Serial::ByteSink sink) { bus_.serial().set_sink(std::move(sink)); }

    Bus& bus() { return bus_; }
    const Bus& bus() const { return bus_; }
    Sm83& cpu() { return cpu_; }
    const Sm83& cpu() const { return cpu_; }

    std::vector<u8> save_state();
    bool load_state(std::span<const u8> data);

  private:
    Bus bus_;
    Sm83 cpu_;
};

}  // namespace gb
