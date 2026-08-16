#include "core/apu/apu.h"

#include <algorithm>

#include "core/bus.h"

namespace gb {

namespace {

// Bits that read back as 1 regardless of what was written, indexed from 0xFF10.
constexpr std::array<u8, 0x30> kReadMask = {
    0x80, 0x3F, 0x00, 0xFF, 0xBF,  // NR10-NR14
    0xFF, 0x3F, 0x00, 0xFF, 0xBF,  // NR20-NR24
    0x7F, 0xFF, 0x9F, 0xFF, 0xBF,  // NR30-NR34
    0xFF, 0xFF, 0x00, 0x00, 0xBF,  // NR40-NR44
    0x00, 0x00, 0x70,              // NR50-NR52
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

}  // namespace

void Apu::reset() {
    regs_.fill(0);
    wave_.fill(0);
    samples_.clear();
    sample_accum_ = 0;
    frame_step_ = 0;
    enabled_ = false;
}

void Apu::set_sample_rate(u32 hz) {
    sample_rate_ = hz == 0 ? 48000 : hz;
    sample_accum_ = 0;
}

void Apu::div_edge() { frame_step_ = static_cast<u8>((frame_step_ + 1) & 7); }

void Apu::tick(u64 tcycles) {
    const u32 period = static_cast<u32>(kTCyclesPerSecond) / sample_rate_;
    for (u64 i = 0; i < tcycles; ++i) {
        if (++sample_accum_ < period) {
            continue;
        }
        sample_accum_ = 0;
        if (samples_.size() < 96000) {
            samples_.push_back(0.0F);
            samples_.push_back(0.0F);
        }
    }
}

std::size_t Apu::take_samples(float* out, std::size_t max_frames) {
    const std::size_t have = samples_.size() / 2;
    const std::size_t n = std::min(have, max_frames);
    std::copy(samples_.begin(), samples_.begin() + static_cast<std::ptrdiff_t>(n * 2), out);
    samples_.erase(samples_.begin(), samples_.begin() + static_cast<std::ptrdiff_t>(n * 2));
    return n;
}

u8 Apu::read(u16 addr) const {
    if (addr >= 0xFF30) {
        return wave_[static_cast<std::size_t>(addr - 0xFF30)];
    }
    const std::size_t i = static_cast<std::size_t>(addr - 0xFF10);
    if (addr == 0xFF26) {
        const u8 status = static_cast<u8>(enabled_ ? 0x80 : 0x00);
        return static_cast<u8>(status | kReadMask[i]);
    }
    return static_cast<u8>(regs_[i] | kReadMask[i]);
}

void Apu::write(u16 addr, u8 value) {
    if (addr >= 0xFF30) {
        wave_[static_cast<std::size_t>(addr - 0xFF30)] = value;
        return;
    }
    const std::size_t i = static_cast<std::size_t>(addr - 0xFF10);
    if (addr == 0xFF26) {
        enabled_ = (value & 0x80) != 0;
        if (!enabled_) {
            // Powering off zeroes every register except the wave pattern RAM.
            for (std::size_t j = 0; j < 0x16; ++j) {
                regs_[j] = 0;
            }
        }
        return;
    }
    if (!enabled_) {
        return;
    }
    regs_[i] = value;
}

template <typename Ar>
void Apu::visit(Ar& ar) {
    ar(regs_);
    ar(wave_);
    ar(sample_rate_);
    ar(sample_accum_);
    ar(frame_step_);
    ar(enabled_);
}

GB_INSTANTIATE_VISIT(Apu);

}  // namespace gb
