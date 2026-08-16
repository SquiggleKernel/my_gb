#include "core/timer.h"

#include "core/bus.h"
#include "core/irq.h"

namespace gb {

void Timer::reset() {
    counter_ = 0xABCC;
    tima_ = 0;
    tma_ = 0;
    tac_ = 0xF8;
    reload_delay_ = 0;
    reloaded_ = false;
}

void Timer::tick(u64 tcycles) {
    for (u64 i = 0; i < tcycles; ++i) {
        step_one();
    }
}

u8 Timer::read(u16 addr) const {
    switch (addr) {
        case 0xFF04:
            return static_cast<u8>(counter_ >> 8);
        case 0xFF05:
            return tima_;
        case 0xFF06:
            return tma_;
        case 0xFF07:
            return static_cast<u8>(tac_ | 0xF8);
        default:
            return 0xFF;
    }
}

void Timer::write(u16 addr, u8 value) {
    switch (addr) {
        case 0xFF04:
            set_counter(0);
            break;
        case 0xFF05:
            // A write on the reload cycle itself loses to TMA; a write anywhere else in the
            // window cancels the reload and the interrupt (blargg tima_write_reloading).
            if (!reloaded_) {
                tima_ = value;
                reload_delay_ = 0;
            }
            break;
        case 0xFF06:
            tma_ = value;
            if (reloaded_) {
                tima_ = value;
            }
            break;
        case 0xFF07: {
            const bool before = edge_input();
            tac_ = static_cast<u8>(value & 0x07);
            detect_edge(before);
            break;
        }
        default:
            break;
    }
}

void Timer::set_counter(u16 value) {
    const bool before = edge_input();
    const bool div_bit_before = (counter_ & 0x0010) != 0;
    counter_ = value;
    detect_edge(before);
    // The APU frame sequencer is clocked by DIV bit 4 falling, which is why
    // resetting DIV can step the envelope and length counters.
    if (div_bit_before && (counter_ & 0x0010) == 0 && bus_ != nullptr) {
        bus_->apu().div_edge();
    }
}

bool Timer::edge_input() const {
    static constexpr int kSelectBit[4] = {9, 3, 5, 7};
    if ((tac_ & 0x04) == 0) {
        return false;
    }
    return ((counter_ >> kSelectBit[tac_ & 0x03]) & 1) != 0;
}

void Timer::detect_edge(bool before) {
    if (!before || edge_input()) {
        return;
    }
    tima_ = static_cast<u8>(tima_ + 1);
    if (tima_ == 0) {
        reload_delay_ = 4;
    }
}

void Timer::step_one() {
    reloaded_ = false;
    if (reload_delay_ != 0) {
        reload_delay_ = static_cast<u8>(reload_delay_ - 1);
        if (reload_delay_ == 0) {
            tima_ = tma_;
            reloaded_ = true;
            if (bus_ != nullptr) {
                bus_->request_irq(kIrqTimer);
            }
        }
    }
    set_counter(static_cast<u16>(counter_ + 1));
}

template <typename Ar>
void Timer::visit(Ar& ar) {
    ar(counter_);
    ar(tima_);
    ar(tma_);
    ar(tac_);
    ar(reload_delay_);
    ar(reloaded_);
}

GB_INSTANTIATE_VISIT(Timer);

}  // namespace gb
