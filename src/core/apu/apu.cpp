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

constexpr std::size_t kLastPoweredReg = 0x15;

void clock_length(u16& length, bool length_on, bool& enabled) {
    if (!length_on || length == 0) {
        return;
    }
    --length;
    if (length == 0) {
        enabled = false;
    }
}

// A silent channel still holds its DAC at the level for digital 0, which is why
// muting by clearing NRx2 pops but muting by disabling the channel does not.
float dac_level(u8 sample, bool dac_on) {
    if (!dac_on) {
        return 0.0F;
    }
    return static_cast<float>(sample) / 7.5F - 1.0F;
}

}  // namespace

void Apu::reset() {
    regs_.fill(0);
    wave_.fill(0);
    samples_.clear();
    ch1_.reset();
    ch2_.reset();
    ch3_.reset();
    ch4_.reset();
    sample_accum_ = 0;
    frame_step_ = 0;
    div_bit_ = false;
    enabled_ = false;
}

void Apu::set_sample_rate(u32 hz) {
    sample_rate_ = hz == 0 ? 48000 : hz;
    sample_accum_ = 0;
}

void Apu::div_edge() { sync_frame_sequencer(); }

void Apu::sync_frame_sequencer() {
    if (bus_ == nullptr) {
        return;
    }
    // DIV bit 4 is bit 12 of the internal divider. Deriving the edge from the
    // counter rather than from the call keeps the sequencer locked to DIV
    // whether it rolled over or was reset by a write.
    const bool bit = (bus_->timer().div_counter() & 0x1000) != 0;
    if (div_bit_ && !bit) {
        step_frame_sequencer();
    }
    div_bit_ = bit;
}

void Apu::step_frame_sequencer() {
    if (!enabled_) {
        return;
    }
    const u8 step = frame_step_;
    frame_step_ = static_cast<u8>((step + 1) & 7);
    if ((step & 1) == 0) {
        clock_length(ch1_.length, ch1_.length_on, ch1_.enabled);
        clock_length(ch2_.length, ch2_.length_on, ch2_.enabled);
        clock_length(ch3_.length, ch3_.length_on, ch3_.enabled);
        clock_length(ch4_.length, ch4_.length_on, ch4_.enabled);
    }
    if (step == 2 || step == 6) {
        ch1_.sweep_clock();
    }
    if (step == 7) {
        ch1_.env.clock();
        ch2_.env.clock();
        ch4_.env.clock();
    }
}

void Apu::tick(u64 tcycles) {
    sync_frame_sequencer();
    if (enabled_ && tcycles != 0) {
        const u32 n = static_cast<u32>(tcycles);
        ch1_.step(n);
        ch2_.step(n);
        if (ch3_.enabled) {
            ch3_.step(n, wave_);
        }
        ch4_.step(n);
    }
    const u32 period = static_cast<u32>(kTCyclesPerSecond) / sample_rate_;
    for (u64 i = 0; i < tcycles; ++i) {
        if (++sample_accum_ < period) {
            continue;
        }
        sample_accum_ = 0;
        if (samples_.size() < 96000) {
            emit_sample();
        }
    }
}

void Apu::emit_sample() {
    float left = 0.0F;
    float right = 0.0F;
    if (enabled_) {
        const float out[4] = {
            dac_level(ch1_.output(), ch1_.dac),
            dac_level(ch2_.output(), ch2_.dac),
            dac_level(ch3_.output(), ch3_.dac),
            dac_level(ch4_.output(), ch4_.dac),
        };
        const u8 pan = regs_[0x15];
        for (int i = 0; i < 4; ++i) {
            if ((pan & (0x10 << i)) != 0) {
                left += out[i];
            }
            if ((pan & (0x01 << i)) != 0) {
                right += out[i];
            }
        }
        const u8 master = regs_[0x14];
        left *= static_cast<float>(((master >> 4) & 0x07) + 1) / 32.0F;
        right *= static_cast<float>((master & 0x07) + 1) / 32.0F;
    }
    samples_.push_back(left);
    samples_.push_back(right);
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
        return read_wave(addr);
    }
    const std::size_t i = static_cast<std::size_t>(addr - 0xFF10);
    if (addr == 0xFF26) {
        u8 status = enabled_ ? 0x80 : 0x00;
        status = static_cast<u8>(status | (ch1_.enabled ? 0x01 : 0x00));
        status = static_cast<u8>(status | (ch2_.enabled ? 0x02 : 0x00));
        status = static_cast<u8>(status | (ch3_.enabled ? 0x04 : 0x00));
        status = static_cast<u8>(status | (ch4_.enabled ? 0x08 : 0x00));
        return static_cast<u8>(status | kReadMask[i]);
    }
    return static_cast<u8>(regs_[i] | kReadMask[i]);
}

u8 Apu::read_wave(u16 addr) const {
    if (!ch3_.enabled) {
        return wave_[static_cast<std::size_t>(addr - 0xFF30)];
    }
    // While the channel plays, the CPU only sees wave RAM on the cycle the
    // channel is latching a byte (blargg 09-wave read while on).
    if (!ch3_.on_fetch()) {
        return 0xFF;
    }
    return wave_[static_cast<std::size_t>(ch3_.fetch_index())];
}

void Apu::write_wave(u16 addr, u8 value) {
    if (!ch3_.enabled) {
        wave_[static_cast<std::size_t>(addr - 0xFF30)] = value;
        return;
    }
    if (!ch3_.on_fetch()) {
        return;
    }
    wave_[static_cast<std::size_t>(ch3_.fetch_index())] = value;
}

void Apu::corrupt_wave() {
    const std::size_t idx = static_cast<std::size_t>(((ch3_.position + 1) & 31) >> 1);
    if (idx < 4) {
        wave_[0] = wave_[idx];
        return;
    }
    const std::size_t base = idx & ~std::size_t{3};
    for (std::size_t j = 0; j < 4; ++j) {
        wave_[j] = wave_[base + j];
    }
}

bool Apu::write_control(u8 value, u16 max_length, u16& length, bool& length_on, bool& enabled) {
    const bool trigger = (value & 0x80) != 0;
    const bool was_on = length_on;
    length_on = (value & 0x40) != 0;
    // Enabling length while the next sequencer step is not a length step steals
    // one clock (blargg 03-trigger).
    if (!next_step_clocks_length() && !was_on && length_on && length > 0) {
        --length;
        if (length == 0 && !trigger) {
            enabled = false;
        }
    }
    if (trigger && length == 0) {
        length = max_length;
        if (length_on && !next_step_clocks_length()) {
            --length;
        }
    }
    return trigger;
}

void Apu::write_powered_off(u16 addr, u8 value) {
    // DMG keeps the four length loads writable with the APU off; the duty and
    // volume bits sharing those registers stay ignored (blargg 08-len ctr
    // during power).
    switch (addr) {
        case 0xFF11:
            ch1_.length = static_cast<u16>(64 - (value & 0x3F));
            break;
        case 0xFF16:
            ch2_.length = static_cast<u16>(64 - (value & 0x3F));
            break;
        case 0xFF1B:
            ch3_.length = static_cast<u16>(256 - value);
            break;
        case 0xFF20:
            ch4_.length = static_cast<u16>(64 - (value & 0x3F));
            break;
        default:
            break;
    }
}

void Apu::power_off() {
    for (std::size_t i = 0; i <= kLastPoweredReg; ++i) {
        regs_[i] = 0;
    }
    const u16 l1 = ch1_.length;
    const u16 l2 = ch2_.length;
    const u16 l3 = ch3_.length;
    const u16 l4 = ch4_.length;
    ch1_.reset();
    ch2_.reset();
    ch3_.reset();
    ch4_.reset();
    // A DMG power cycle leaves the length counters alone (blargg 11-regs after
    // power); only the CGB clears them.
    ch1_.length = l1;
    ch2_.length = l2;
    ch3_.length = l3;
    ch4_.length = l4;
    enabled_ = false;
}

void Apu::power_on() {
    enabled_ = true;
    frame_step_ = 0;
    ch1_.duty_pos = 0;
    ch2_.duty_pos = 0;
    ch3_.position = 0;
}

void Apu::write(u16 addr, u8 value) {
    if (addr >= 0xFF30) {
        write_wave(addr, value);
        return;
    }
    if (addr == 0xFF26) {
        const bool on = (value & 0x80) != 0;
        if (on && !enabled_) {
            power_on();
        } else if (!on && enabled_) {
            power_off();
        }
        return;
    }
    if (!enabled_) {
        write_powered_off(addr, value);
        return;
    }

    const std::size_t i = static_cast<std::size_t>(addr - 0xFF10);
    if (i <= kLastPoweredReg) {
        regs_[i] = value;
    }

    switch (addr) {
        case 0xFF10: {
            ch1_.sweep_pace = static_cast<u8>((value >> 4) & 0x07);
            const bool negate = (value & 0x08) != 0;
            // Clearing negate after a calculation kills the channel
            // (blargg 05-sweep details).
            if (!negate && ch1_.negate_used) {
                ch1_.enabled = false;
            }
            ch1_.sweep_negate = negate;
            ch1_.sweep_shift = static_cast<u8>(value & 0x07);
            break;
        }
        case 0xFF11:
            ch1_.duty = static_cast<u8>(value >> 6);
            ch1_.length = static_cast<u16>(64 - (value & 0x3F));
            break;
        case 0xFF12:
            ch1_.env.initial = static_cast<u8>(value >> 4);
            ch1_.env.add = (value & 0x08) != 0;
            ch1_.env.period = static_cast<u8>(value & 0x07);
            ch1_.dac = (value & 0xF8) != 0;
            if (!ch1_.dac) {
                ch1_.enabled = false;
            }
            break;
        case 0xFF13:
            ch1_.freq = static_cast<u16>((ch1_.freq & 0x0700) | value);
            break;
        case 0xFF14:
            ch1_.freq = static_cast<u16>((ch1_.freq & 0x00FF) | ((value & 0x07) << 8));
            if (write_control(value, 64, ch1_.length, ch1_.length_on, ch1_.enabled)) {
                ch1_.trigger(true);
            }
            break;

        case 0xFF16:
            ch2_.duty = static_cast<u8>(value >> 6);
            ch2_.length = static_cast<u16>(64 - (value & 0x3F));
            break;
        case 0xFF17:
            ch2_.env.initial = static_cast<u8>(value >> 4);
            ch2_.env.add = (value & 0x08) != 0;
            ch2_.env.period = static_cast<u8>(value & 0x07);
            ch2_.dac = (value & 0xF8) != 0;
            if (!ch2_.dac) {
                ch2_.enabled = false;
            }
            break;
        case 0xFF18:
            ch2_.freq = static_cast<u16>((ch2_.freq & 0x0700) | value);
            break;
        case 0xFF19:
            ch2_.freq = static_cast<u16>((ch2_.freq & 0x00FF) | ((value & 0x07) << 8));
            if (write_control(value, 64, ch2_.length, ch2_.length_on, ch2_.enabled)) {
                ch2_.trigger(false);
            }
            break;

        case 0xFF1A:
            ch3_.dac = (value & 0x80) != 0;
            if (!ch3_.dac) {
                ch3_.enabled = false;
            }
            break;
        case 0xFF1B:
            ch3_.length = static_cast<u16>(256 - value);
            break;
        case 0xFF1C:
            ch3_.level = static_cast<u8>((value >> 5) & 0x03);
            break;
        case 0xFF1D:
            ch3_.freq = static_cast<u16>((ch3_.freq & 0x0700) | value);
            break;
        case 0xFF1E: {
            const bool playing = ch3_.enabled;
            ch3_.freq = static_cast<u16>((ch3_.freq & 0x00FF) | ((value & 0x07) << 8));
            if (write_control(value, 256, ch3_.length, ch3_.length_on, ch3_.enabled)) {
                // Re-triggering on the latch cycle drags the byte being read
                // down into the first four (blargg 10-wave trigger while on).
                if (playing && ch3_.on_fetch()) {
                    corrupt_wave();
                }
                ch3_.trigger(wave_);
            }
            break;
        }

        case 0xFF20:
            ch4_.length = static_cast<u16>(64 - (value & 0x3F));
            break;
        case 0xFF21:
            ch4_.env.initial = static_cast<u8>(value >> 4);
            ch4_.env.add = (value & 0x08) != 0;
            ch4_.env.period = static_cast<u8>(value & 0x07);
            ch4_.dac = (value & 0xF8) != 0;
            if (!ch4_.dac) {
                ch4_.enabled = false;
            }
            break;
        case 0xFF22:
            ch4_.clock_shift = static_cast<u8>(value >> 4);
            ch4_.width = (value & 0x08) != 0;
            ch4_.divisor = static_cast<u8>(value & 0x07);
            break;
        case 0xFF23:
            if (write_control(value, 64, ch4_.length, ch4_.length_on, ch4_.enabled)) {
                ch4_.trigger();
            }
            break;

        default:
            break;
    }
}

template <typename Ar>
void Apu::visit(Ar& ar) {
    ar(regs_);
    ar(wave_);
    ch1_.visit(ar);
    ch2_.visit(ar);
    ch3_.visit(ar);
    ch4_.visit(ar);
    ar(sample_rate_);
    ar(sample_accum_);
    ar(frame_step_);
    ar(div_bit_);
    ar(enabled_);
}

GB_INSTANTIATE_VISIT(Apu);

}  // namespace gb
