#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include "core/state/serialize.h"
#include "core/types.h"

namespace gb {

class Bus;

class Apu {
  public:
    void attach(Bus* bus) { bus_ = bus; }
    void reset();

    void tick(u64 tcycles);

    // The frame sequencer is driven by the falling edge of DIV bit 4, not by a
    // counter of its own; the timer calls this.
    void div_edge();

    u8 read(u16 addr) const;
    void write(u16 addr, u8 value);

    void set_sample_rate(u32 hz);
    // Stereo interleaved. Returns frames written.
    std::size_t take_samples(float* out, std::size_t max_frames);
    void clear_samples() { samples_.clear(); }

    template <typename Ar>
    void visit(Ar& ar);

  private:
    Bus* bus_ = nullptr;

    std::array<u8, 0x30> regs_{};
    std::array<u8, 0x10> wave_{};
    std::vector<float> samples_;
    u32 sample_rate_ = 48000;
    u32 sample_accum_ = 0;
    u8 frame_step_ = 0;
    bool enabled_ = false;
};

}  // namespace gb
