#pragma once

#include "core/state/serialize.h"
#include "core/types.h"

namespace gb {

class Bus;

enum Button : u8 {
    kBtnRight = 0x01,
    kBtnLeft = 0x02,
    kBtnUp = 0x04,
    kBtnDown = 0x08,
    kBtnA = 0x10,
    kBtnB = 0x20,
    kBtnSelect = 0x40,
    kBtnStart = 0x80,
};

class Joypad {
  public:
    void attach(Bus* bus) { bus_ = bus; }
    void reset();

    // Bit set means pressed; the register inverts this.
    void set_buttons(u8 pressed);
    u8 buttons() const { return pressed_; }

    u8 read() const;
    void write(u8 value);

    template <typename Ar>
    void visit(Ar& ar);

  private:
    Bus* bus_ = nullptr;
    u8 pressed_ = 0;
    u8 select_ = 0x30;
};

}  // namespace gb
