#include "core/apu/channel.h"

#include <cstdlib>

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
    while (tcycles >= timer) {
        tcycles -= timer;
        timer = period_tcycles();
        position = static_cast<u8>((position + 1) & 31);
        sample = ram[position >> 1];
    }
    timer = static_cast<u16>(timer - tcycles);
}

int gb_env(const char* name, int def) {
    const char* v = std::getenv(name);
    return v != nullptr ? std::atoi(v) : def;
}

bool WaveChannel::on_fetch() const {
    static const u32 kW = static_cast<u32>(gb_env("GB_W", 2));
    static const u32 kS = static_cast<u32>(gb_env("GB_S", 0));
    const u32 p = period_tcycles();
    const u32 elapsed = p - static_cast<u32>(timer);
    return ((elapsed + kS) % p) < kW;
}

u8 WaveChannel::fetch_index() const {
    static const u32 kQ = static_cast<u32>(gb_env("GB_Q", 0));
    return static_cast<u8>(((static_cast<u32>(position) + kQ) & 31) >> 1);
}

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
    timer = static_cast<u16>(period_tcycles() + static_cast<u16>(gb_env("GB_D", 0)));
    sample = ram[0];
}

}  // namespace gb
