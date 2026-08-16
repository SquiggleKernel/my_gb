#include "core/cpu/sm83.h"

#include <cstddef>

#include "core/bus.h"
#include "core/cpu/opcodes.inl"

namespace gb {

void Sm83::reset() {
    r_.a = 0x01;
    r_.f = 0xB0;
    r_.b = 0x00;
    r_.c = 0x13;
    r_.d = 0x00;
    r_.e = 0xD8;
    r_.h = 0x01;
    r_.l = 0x4D;
    r_.sp = 0xFFFE;
    r_.pc = 0x0100;
    ime_ = false;
    ime_pending_ = false;
    halted_ = false;
    stopped_ = false;
    halt_bug_ = false;
}

void Sm83::step() {
    if (stopped_) {
        bus_->tick(4);
        return;
    }
    if (halted_) {
        if (bus_->pending_irqs() == 0) {
            bus_->tick(4);
            return;
        }
        halted_ = false;
    }
    if (service_interrupt()) {
        return;
    }
    // EI takes effect only once the instruction after it has run, so the enable
    // lands after the dispatch check above (blargg ei_sequence).
    if (ime_pending_) {
        ime_ = true;
        ime_pending_ = false;
    }
    execute(fetch8());
}

u8 Sm83::fetch8() {
    const u8 value = bus_->read(r_.pc);
    if (halt_bug_) {
        halt_bug_ = false;
    } else {
        r_.pc = static_cast<u16>(r_.pc + 1);
    }
    return value;
}

u16 Sm83::fetch16() {
    const u8 lo = fetch8();
    const u8 hi = fetch8();
    return static_cast<u16>((hi << 8) | lo);
}

void Sm83::push16(u16 v) {
    bus_->tick(4);
    r_.sp = static_cast<u16>(r_.sp - 1);
    bus_->write(r_.sp, static_cast<u8>(v >> 8));
    r_.sp = static_cast<u16>(r_.sp - 1);
    bus_->write(r_.sp, static_cast<u8>(v & 0xFF));
}

u16 Sm83::pop16() {
    const u8 lo = bus_->read(r_.sp);
    r_.sp = static_cast<u16>(r_.sp + 1);
    const u8 hi = bus_->read(r_.sp);
    r_.sp = static_cast<u16>(r_.sp + 1);
    return static_cast<u16>((hi << 8) | lo);
}

bool Sm83::service_interrupt() {
    if (!ime_ || bus_->pending_irqs() == 0) {
        return false;
    }
    ime_ = false;
    ime_pending_ = false;
    bus_->tick(8);
    r_.sp = static_cast<u16>(r_.sp - 1);
    bus_->write(r_.sp, static_cast<u8>(r_.pc >> 8));
    // The vector is decided only after the high byte has been written, so a
    // stack that runs over IE at 0xFFFF can retarget or cancel the dispatch.
    const u8 pending = bus_->pending_irqs();
    r_.sp = static_cast<u16>(r_.sp - 1);
    bus_->write(r_.sp, static_cast<u8>(r_.pc & 0xFF));
    u16 vector = 0x0000;
    for (u8 bit = 0; bit < 5; ++bit) {
        const u8 mask = static_cast<u8>(1u << bit);
        if ((pending & mask) != 0) {
            bus_->clear_irq(mask);
            vector = static_cast<u16>(0x40 + bit * 8);
            break;
        }
    }
    r_.pc = vector;
    bus_->tick(4);
    return true;
}

void Sm83::execute(u8 opcode) { kMainOps[static_cast<std::size_t>(opcode)](*this); }

void Sm83::execute_cb(u8 opcode) { kCbOps[static_cast<std::size_t>(opcode)](*this); }

template <typename Ar>
void Sm83::visit(Ar& ar) {
    ar(r_.a);
    ar(r_.f);
    ar(r_.b);
    ar(r_.c);
    ar(r_.d);
    ar(r_.e);
    ar(r_.h);
    ar(r_.l);
    ar(r_.sp);
    ar(r_.pc);
    ar(ime_);
    ar(ime_pending_);
    ar(halted_);
    ar(stopped_);
    ar(halt_bug_);
}

GB_INSTANTIATE_VISIT(Sm83);

}  // namespace gb
