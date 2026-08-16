#pragma once

#include <array>

#include "core/types.h"

namespace gb {

// Shared by both squares and the noise channel. A period of 0 never steps the
// volume, but the timer still reloads as if it were 8.
struct Envelope {
    u8 initial = 0;
    u8 period = 0;
    u8 volume = 0;
    u8 timer = 0;
    bool add = false;
    bool running = false;

    void reset();
    void trigger();
    void clock();

    template <typename Ar>
    void visit(Ar& ar) {
        ar(initial);
        ar(period);
        ar(volume);
        ar(timer);
        ar(add);
        ar(running);
    }
};

struct SquareChannel {
    Envelope env;
    u16 freq = 0;
    u16 timer = 0;
    u16 length = 0;
    u16 shadow = 0;
    u8 duty = 0;
    u8 duty_pos = 0;
    u8 sweep_pace = 0;
    u8 sweep_shift = 0;
    u8 sweep_timer = 0;
    bool sweep_negate = false;
    bool sweep_on = false;
    bool negate_used = false;
    bool length_on = false;
    bool enabled = false;
    bool dac = false;

    void reset();
    void step(u32 tcycles);
    void trigger(bool with_sweep);
    void sweep_clock();
    u16 sweep_calculate();
    u16 period_tcycles() const;
    u8 output() const;

    template <typename Ar>
    void visit(Ar& ar) {
        env.visit(ar);
        ar(freq);
        ar(timer);
        ar(length);
        ar(shadow);
        ar(duty);
        ar(duty_pos);
        ar(sweep_pace);
        ar(sweep_shift);
        ar(sweep_timer);
        ar(sweep_negate);
        ar(sweep_on);
        ar(negate_used);
        ar(length_on);
        ar(enabled);
        ar(dac);
    }
};

struct WaveChannel {
    u16 freq = 0;
    u16 timer = 0;
    u16 length = 0;
    u8 level = 0;
    u8 position = 0;
    u8 sample = 0;
    bool length_on = false;
    bool enabled = false;
    bool dac = false;
    // Set for the duration of the tick in which a byte was latched. The CPU
    // resolves its access at the end of the same tick, so this is the window.
    bool latched = false;

    void reset();
    void step(u32 tcycles, const std::array<u8, 0x10>& ram);
    void trigger(const std::array<u8, 0x10>& ram);
    // True while the channel is on the byte it is about to latch, which is the
    // only moment the CPU shares the wave RAM bus on DMG.
    bool on_fetch() const;
    u8 fetch_index() const;
    u16 period_tcycles() const;
    u8 output() const;

    template <typename Ar>
    void visit(Ar& ar) {
        ar(freq);
        ar(timer);
        ar(length);
        ar(level);
        ar(position);
        ar(sample);
        ar(length_on);
        ar(enabled);
        ar(dac);
        ar(latched);
    }
};

struct NoiseChannel {
    Envelope env;
    u32 timer = 0;
    u16 lfsr = 0x7FFF;
    u16 length = 0;
    u8 clock_shift = 0;
    u8 divisor = 0;
    bool width = false;
    bool length_on = false;
    bool enabled = false;
    bool dac = false;

    void reset();
    void step(u32 tcycles);
    void trigger();
    u32 period_tcycles() const;
    u8 output() const;

    template <typename Ar>
    void visit(Ar& ar) {
        env.visit(ar);
        ar(timer);
        ar(lfsr);
        ar(length);
        ar(clock_shift);
        ar(divisor);
        ar(width);
        ar(length_on);
        ar(enabled);
        ar(dac);
    }
};

}  // namespace gb
