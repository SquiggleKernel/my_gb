#pragma once

#include <span>
#include <string>
#include <vector>

#include "core/bus.h"
#include "core/cpu/sm83.h"
#include "core/state/rewind.h"
#include "core/types.h"

namespace gb {

class Gb {
  public:
    Gb();

    // Components hold Bus back-pointers wired up in the constructor, so a copy
    // or move would leave them pointing at the old machine.
    Gb(const Gb&) = delete;
    Gb& operator=(const Gb&) = delete;
    Gb(Gb&&) = delete;
    Gb& operator=(Gb&&) = delete;

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
    // Writes into `out`, reusing its allocation. Rewind takes this path.
    void write_state(std::vector<u8>& out);
    // Leaves the machine untouched when the state is rejected or truncated.
    bool load_state(std::span<const u8> data);

    Rewind& rewind() { return rewind_; }
    void set_rewind_enabled(bool on) { rewind_enabled_ = on; }
    bool rewind_enabled() const { return rewind_enabled_; }
    bool rewind_step_back() { return rewind_.step_back(*this); }

  private:
    // Identifies the machine a state came from, so a state cannot be loaded
    // into a differently configured one.
    u8 cart_tag() const;
    u32 cart_rom_size() const;

    bool read_header(ReadArchive& ar) const;
    void apply_payload(ReadArchive& ar);

    Bus bus_;
    Sm83 cpu_;
    Rewind rewind_;
    std::vector<u8> rollback_;
    bool rewind_enabled_ = false;
};

}  // namespace gb
