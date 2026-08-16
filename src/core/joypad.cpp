#include "core/joypad.h"

#include "core/bus.h"
#include "core/irq.h"

namespace gb {

void Joypad::reset() {
    pressed_ = 0;
    select_ = 0x30;
}

void Joypad::set_buttons(u8 pressed) {
    const u8 before = read();
    pressed_ = pressed;
    const u8 after = read();
    // The interrupt comes from the input lines themselves, so it only fires for a press on a
    // currently selected row.
    if (static_cast<u8>(before & ~after & 0x0F) != 0 && bus_ != nullptr) {
        bus_->request_irq(kIrqJoypad);
    }
}

u8 Joypad::read() const {
    u8 value = static_cast<u8>(0xC0 | select_ | 0x0F);
    if ((select_ & 0x10) == 0) {
        value = static_cast<u8>(value & ~(pressed_ & 0x0F));
    }
    if ((select_ & 0x20) == 0) {
        value = static_cast<u8>(value & ~((pressed_ >> 4) & 0x0F));
    }
    return value;
}

void Joypad::write(u8 value) { select_ = static_cast<u8>(value & 0x30); }

template <typename Ar>
void Joypad::visit(Ar& ar) {
    ar(pressed_);
    ar(select_);
}

GB_INSTANTIATE_VISIT(Joypad);

}  // namespace gb
