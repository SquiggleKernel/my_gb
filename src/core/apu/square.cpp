#include "core/apu/channel.h"

namespace gb {

namespace {

constexpr u8 kDutyTable[4][8] = {
    {0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 1, 1, 1},
    {0, 1, 1, 1, 1, 1, 1, 0},
};

}  // namespace

void Envelope::reset() { *this = Envelope{}; }

void Envelope::trigger() {
    volume = initial;
    timer = period == 0 ? 8 : period;
    running = true;
}

void Envelope::clock() {
    if (timer > 0) {
        timer = static_cast<u8>(timer - 1);
    }
    if (timer != 0) {
        return;
    }
    timer = period == 0 ? 8 : period;
    if (!running || period == 0) {
        return;
    }
    const int next = static_cast<int>(volume) + (add ? 1 : -1);
    if (next < 0 || next > 15) {
        running = false;
        return;
    }
    volume = static_cast<u8>(next);
}

void SquareChannel::reset() {
    *this = SquareChannel{};
    timer = period_tcycles();
}

u16 SquareChannel::period_tcycles() const { return static_cast<u16>((2048 - freq) * 4); }

void SquareChannel::step(u32 tcycles) {
    while (tcycles >= timer) {
        tcycles -= timer;
        timer = period_tcycles();
        duty_pos = static_cast<u8>((duty_pos + 1) & 7);
    }
    timer = static_cast<u16>(timer - tcycles);
}

u8 SquareChannel::output() const {
    if (!enabled) {
        return 0;
    }
    return kDutyTable[duty][duty_pos] != 0 ? env.volume : 0;
}

u16 SquareChannel::sweep_calculate() {
    const u16 delta = static_cast<u16>(shadow >> sweep_shift);
    u16 next = 0;
    if (sweep_negate) {
        next = static_cast<u16>(shadow - delta);
        negate_used = true;
    } else {
        next = static_cast<u16>(shadow + delta);
    }
    if (next > 2047) {
        enabled = false;
    }
    return next;
}

void SquareChannel::sweep_clock() {
    if (sweep_timer > 0) {
        sweep_timer = static_cast<u8>(sweep_timer - 1);
    }
    if (sweep_timer != 0) {
        return;
    }
    sweep_timer = sweep_pace == 0 ? 8 : sweep_pace;
    if (!sweep_on || sweep_pace == 0) {
        return;
    }
    const u16 next = sweep_calculate();
    // A shift of 0 still runs the overflow check but never writes back
    // (blargg 04-sweep, "If shift=0, doesn't update").
    if (next > 2047 || sweep_shift == 0) {
        return;
    }
    shadow = next;
    freq = next;
    sweep_calculate();
}

void SquareChannel::trigger(bool with_sweep) {
    enabled = dac;
    env.trigger();
    timer = period_tcycles();
    if (!with_sweep) {
        return;
    }
    shadow = freq;
    sweep_timer = sweep_pace == 0 ? 8 : sweep_pace;
    sweep_on = sweep_pace != 0 || sweep_shift != 0;
    negate_used = false;
    // The check runs before a single sample is produced, so an overflowing
    // frequency never sounds (blargg 06-overflow on trigger).
    if (sweep_shift != 0) {
        sweep_calculate();
    }
}

}  // namespace gb
