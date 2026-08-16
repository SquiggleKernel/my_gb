#include "core/apu/channel.h"

namespace gb {

namespace {

constexpr u16 kDivisor[8] = {8, 16, 32, 48, 64, 80, 96, 112};

}  // namespace

void NoiseChannel::reset() {
    *this = NoiseChannel{};
    timer = period_tcycles();
}

u32 NoiseChannel::period_tcycles() const {
    return static_cast<u32>(kDivisor[divisor]) << clock_shift;
}

void NoiseChannel::step(u32 tcycles) {
    while (tcycles >= timer) {
        tcycles -= timer;
        timer = period_tcycles();
        const u16 feedback = static_cast<u16>((lfsr ^ (lfsr >> 1)) & 1);
        lfsr = static_cast<u16>((lfsr >> 1) | static_cast<u16>(feedback << 14));
        if (width) {
            lfsr = static_cast<u16>((lfsr & 0xFFBF) | static_cast<u16>(feedback << 6));
        }
    }
    timer -= tcycles;
}

u8 NoiseChannel::output() const {
    if (!enabled) {
        return 0;
    }
    return (lfsr & 1) == 0 ? env.volume : 0;
}

void NoiseChannel::trigger() {
    enabled = dac;
    env.trigger();
    timer = period_tcycles();
    lfsr = 0x7FFF;
}

}  // namespace gb
