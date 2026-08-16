#pragma once

#include "core/state/serialize.h"
#include "core/types.h"

namespace gb {

class Bus;
struct Sm83Ops;

inline constexpr u8 kFlagZ = 0x80;
inline constexpr u8 kFlagN = 0x40;
inline constexpr u8 kFlagH = 0x20;
inline constexpr u8 kFlagC = 0x10;

struct Regs {
    u8 a = 0, f = 0;
    u8 b = 0, c = 0;
    u8 d = 0, e = 0;
    u8 h = 0, l = 0;
    u16 sp = 0;
    u16 pc = 0;

    u16 af() const { return static_cast<u16>((a << 8) | f); }
    u16 bc() const { return static_cast<u16>((b << 8) | c); }
    u16 de() const { return static_cast<u16>((d << 8) | e); }
    u16 hl() const { return static_cast<u16>((h << 8) | l); }

    void set_af(u16 v) {
        a = static_cast<u8>(v >> 8);
        f = static_cast<u8>(v & 0xF0);
    }
    void set_bc(u16 v) {
        b = static_cast<u8>(v >> 8);
        c = static_cast<u8>(v);
    }
    void set_de(u16 v) {
        d = static_cast<u8>(v >> 8);
        e = static_cast<u8>(v);
    }
    void set_hl(u16 v) {
        h = static_cast<u8>(v >> 8);
        l = static_cast<u8>(v);
    }
};

class Sm83 {
  public:
    void attach(Bus* bus) { bus_ = bus; }
    void reset();

    // Runs one instruction, or services an interrupt, or burns a cycle while
    // halted. Time advances through Bus, never through a return value.
    void step();

    Regs& regs() { return r_; }
    const Regs& regs() const { return r_; }

    bool ime() const { return ime_; }
    bool halted() const { return halted_; }
    bool stopped() const { return stopped_; }

    template <typename Ar>
    void visit(Ar& ar);

  private:
    // The opcode handlers in opcodes.inl are generated as members of Sm83Ops.
    friend struct Sm83Ops;

    u8 fetch8();
    u16 fetch16();
    void push16(u16 v);
    u16 pop16();
    bool service_interrupt();
    void execute(u8 opcode);
    void execute_cb(u8 opcode);

    Bus* bus_ = nullptr;
    Regs r_;
    bool ime_ = false;
    bool ime_pending_ = false;
    bool halted_ = false;
    bool stopped_ = false;
    // HALT with IME=0 and a pending interrupt fails to advance PC once.
    bool halt_bug_ = false;
};

}  // namespace gb
