#pragma once

#include "core/types.h"

namespace gb {

enum Irq : u8 {
    kIrqVBlank = 0x01,
    kIrqStat = 0x02,
    kIrqTimer = 0x04,
    kIrqSerial = 0x08,
    kIrqJoypad = 0x10,
};

}  // namespace gb
