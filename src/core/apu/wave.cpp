#include "core/apu/channel.h"

namespace gb {

namespace {

// NR32 levels 0-3 map to mute, full, half and quarter volume.
constexpr u8 kLevelShift[4] = {4, 0, 1, 2};

}  // namespace

void WaveChannel::reset() {
    *this = WaveChannel{};
    timer = period_tcycles();
}

u16 WaveChannel::period_tcycles() const { return static_cast<u16>((2048 - freq) * 2); }

void WaveChannel::step(u32 tcycles, const std::array<u8, 0x10>& ram) {
    latched = false;
    while (tcycles >= timer) {
        tcycles -= timer;
        timer = period_tcycles();
        position = static_cast<u8>((position + 1) & 31);
        sample = ram[position >> 1];
        latched = true;
    }
    timer = static_cast<u16>(timer - tcycles);
}

bool WaveChannel::on_fetch() const { return latched; }

u8 WaveChannel::fetch_index() const { return static_cast<u8>(position >> 1); }

u8 WaveChannel::output() const {
    if (!enabled) {
        return 0;
    }
    const u8 nibble =
        (position & 1) != 0 ? static_cast<u8>(sample & 0x0F) : static_cast<u8>(sample >> 4);
    return static_cast<u8>(nibble >> kLevelShift[level]);
}

void WaveChannel::trigger(const std::array<u8, 0x10>& ram) {
    enabled = dac;
    position = 0;
    timer = period_tcycles();
    sample = ram[0];
}

}  // namespace gb
