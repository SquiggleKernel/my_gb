#include "core/serial.h"

#include "core/bus.h"
#include "core/irq.h"

namespace gb {

namespace {

// The internal clock runs at 8192 Hz, so one bit leaves every 512 T-cycles.
constexpr u16 kBitPeriod = 512;

}  // namespace

void Serial::reset() {
    sb_ = 0;
    sc_ = 0;
    bits_left_ = 0;
    out_byte_ = 0;
    shift_timer_ = 0;
}

void Serial::tick(u64 tcycles) {
    if (bits_left_ == 0 || (sc_ & 0x81) != 0x81) {
        return;
    }
    for (u64 i = 0; i < tcycles; ++i) {
        ++shift_timer_;
        if (shift_timer_ < kBitPeriod) {
            continue;
        }
        shift_timer_ = 0;
        sb_ = static_cast<u8>((sb_ << 1) | 1);
        bits_left_ = static_cast<u8>(bits_left_ - 1);
        if (bits_left_ == 0) {
            sc_ = static_cast<u8>(sc_ & 0x7F);
            if (bus_ != nullptr) {
                bus_->request_irq(kIrqSerial);
            }
            if (sink_) {
                sink_(out_byte_);
            }
            return;
        }
    }
}

u8 Serial::read(u16 addr) const {
    switch (addr) {
        case 0xFF01: return sb_;
        case 0xFF02: return static_cast<u8>(sc_ | 0x7E);
        default: return 0xFF;
    }
}

void Serial::write(u16 addr, u8 value) {
    switch (addr) {
        case 0xFF01:
            sb_ = value;
            break;
        case 0xFF02:
            sc_ = static_cast<u8>(value & 0x81);
            if ((sc_ & 0x80) != 0) {
                bits_left_ = 8;
                out_byte_ = sb_;
                shift_timer_ = 0;
            } else {
                bits_left_ = 0;
            }
            break;
        default: break;
    }
}

template <typename Ar>
void Serial::visit(Ar& ar) {
    ar(sb_);
    ar(sc_);
    ar(bits_left_);
    ar(out_byte_);
    ar(shift_timer_);
}

GB_INSTANTIATE_VISIT(Serial);

}  // namespace gb
