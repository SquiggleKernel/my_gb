#pragma once

#include <string>

#include "core/types.h"

namespace gb {

class Bus;

struct Disasm {
    u16 addr;
    u8 length;
    std::string text;
};

// Reads through Bus::peek() only, so the debugger and the tracer can call this
// on a stopped CPU without perturbing the machine.
Disasm disassemble(const Bus& bus, u16 addr);

}  // namespace gb
