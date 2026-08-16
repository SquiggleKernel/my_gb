#include <array>
#include <cstddef>
#include <utility>

#include "core/bus.h"
#include "core/cpu/sm83.h"

namespace gb {

constexpr bool is_locked_opcode(u8 op) {
    return op == 0xD3 || op == 0xDB || op == 0xDD || op == 0xE3 || op == 0xE4 || op == 0xEB ||
           op == 0xEC || op == 0xED || op == 0xF4 || op == 0xFC || op == 0xFD;
}

struct Sm83Ops {
    template <u8 Op>
    static void exec(Sm83& c);

    template <u8 Op>
    static void exec_cb(Sm83& c);

  private:
    template <u8 Idx>
    static u8 get_r(Sm83& c);
    template <u8 Idx>
    static void set_r(Sm83& c, u8 v);
    template <u8 Idx>
    static u16 get_rp(const Sm83& c);
    template <u8 Idx>
    static void set_rp(Sm83& c, u16 v);
    template <u8 Cc>
    static bool cond(const Sm83& c);
    template <u8 Kind>
    static void alu(Sm83& c, u8 v);
    template <u8 Kind>
    static u8 rotate(Sm83& c, u8 v, bool with_z);

    static u8 carry_in(const Sm83& c);
    static void add8(Sm83& c, u8 v, u8 carry);
    static void sub8(Sm83& c, u8 v, u8 carry, bool store);
    static u8 inc8(Sm83& c, u8 v);
    static u8 dec8(Sm83& c, u8 v);
    static void daa(Sm83& c);
    static void add_sp_flags(Sm83& c, u8 imm);
    static void halt(Sm83& c);
};

template <u8 Idx>
u8 Sm83Ops::get_r(Sm83& c) {
    if constexpr (Idx == 0) {
        return c.r_.b;
    } else if constexpr (Idx == 1) {
        return c.r_.c;
    } else if constexpr (Idx == 2) {
        return c.r_.d;
    } else if constexpr (Idx == 3) {
        return c.r_.e;
    } else if constexpr (Idx == 4) {
        return c.r_.h;
    } else if constexpr (Idx == 5) {
        return c.r_.l;
    } else if constexpr (Idx == 6) {
        return c.bus_->read(c.r_.hl());
    } else {
        return c.r_.a;
    }
}

template <u8 Idx>
void Sm83Ops::set_r(Sm83& c, u8 v) {
    if constexpr (Idx == 0) {
        c.r_.b = v;
    } else if constexpr (Idx == 1) {
        c.r_.c = v;
    } else if constexpr (Idx == 2) {
        c.r_.d = v;
    } else if constexpr (Idx == 3) {
        c.r_.e = v;
    } else if constexpr (Idx == 4) {
        c.r_.h = v;
    } else if constexpr (Idx == 5) {
        c.r_.l = v;
    } else if constexpr (Idx == 6) {
        c.bus_->write(c.r_.hl(), v);
    } else {
        c.r_.a = v;
    }
}

template <u8 Idx>
u16 Sm83Ops::get_rp(const Sm83& c) {
    if constexpr (Idx == 0) {
        return c.r_.bc();
    } else if constexpr (Idx == 1) {
        return c.r_.de();
    } else if constexpr (Idx == 2) {
        return c.r_.hl();
    } else {
        return c.r_.sp;
    }
}

template <u8 Idx>
void Sm83Ops::set_rp(Sm83& c, u16 v) {
    if constexpr (Idx == 0) {
        c.r_.set_bc(v);
    } else if constexpr (Idx == 1) {
        c.r_.set_de(v);
    } else if constexpr (Idx == 2) {
        c.r_.set_hl(v);
    } else {
        c.r_.sp = v;
    }
}

template <u8 Cc>
bool Sm83Ops::cond(const Sm83& c) {
    if constexpr (Cc == 0) {
        return (c.r_.f & kFlagZ) == 0;
    } else if constexpr (Cc == 1) {
        return (c.r_.f & kFlagZ) != 0;
    } else if constexpr (Cc == 2) {
        return (c.r_.f & kFlagC) == 0;
    } else {
        return (c.r_.f & kFlagC) != 0;
    }
}

u8 Sm83Ops::carry_in(const Sm83& c) { return (c.r_.f & kFlagC) != 0 ? u8{1} : u8{0}; }

void Sm83Ops::add8(Sm83& c, u8 v, u8 carry) {
    const int sum = c.r_.a + v + carry;
    const u8 res = static_cast<u8>(sum);
    u8 f = 0;
    if (res == 0) {
        f |= kFlagZ;
    }
    if ((c.r_.a & 0x0F) + (v & 0x0F) + carry > 0x0F) {
        f |= kFlagH;
    }
    if (sum > 0xFF) {
        f |= kFlagC;
    }
    c.r_.a = res;
    c.r_.f = f;
}

void Sm83Ops::sub8(Sm83& c, u8 v, u8 carry, bool store) {
    const int diff = c.r_.a - v - carry;
    const u8 res = static_cast<u8>(diff);
    u8 f = kFlagN;
    if (res == 0) {
        f |= kFlagZ;
    }
    if ((c.r_.a & 0x0F) < (v & 0x0F) + carry) {
        f |= kFlagH;
    }
    if (diff < 0) {
        f |= kFlagC;
    }
    if (store) {
        c.r_.a = res;
    }
    c.r_.f = f;
}

template <u8 Kind>
void Sm83Ops::alu(Sm83& c, u8 v) {
    if constexpr (Kind == 0) {
        add8(c, v, 0);
    } else if constexpr (Kind == 1) {
        add8(c, v, carry_in(c));
    } else if constexpr (Kind == 2) {
        sub8(c, v, 0, true);
    } else if constexpr (Kind == 3) {
        sub8(c, v, carry_in(c), true);
    } else if constexpr (Kind == 4) {
        c.r_.a = static_cast<u8>(c.r_.a & v);
        c.r_.f = static_cast<u8>((c.r_.a == 0 ? kFlagZ : 0) | kFlagH);
    } else if constexpr (Kind == 5) {
        c.r_.a = static_cast<u8>(c.r_.a ^ v);
        c.r_.f = static_cast<u8>(c.r_.a == 0 ? kFlagZ : 0);
    } else if constexpr (Kind == 6) {
        c.r_.a = static_cast<u8>(c.r_.a | v);
        c.r_.f = static_cast<u8>(c.r_.a == 0 ? kFlagZ : 0);
    } else {
        sub8(c, v, 0, false);
    }
}

template <u8 Kind>
u8 Sm83Ops::rotate(Sm83& c, u8 v, bool with_z) {
    [[maybe_unused]] const bool old_carry = (c.r_.f & kFlagC) != 0;
    u8 res = 0;
    bool carry = false;
    if constexpr (Kind == 0) {
        carry = (v & 0x80) != 0;
        res = static_cast<u8>((v << 1) | (carry ? 1 : 0));
    } else if constexpr (Kind == 1) {
        carry = (v & 0x01) != 0;
        res = static_cast<u8>((v >> 1) | (carry ? 0x80 : 0));
    } else if constexpr (Kind == 2) {
        carry = (v & 0x80) != 0;
        res = static_cast<u8>((v << 1) | (old_carry ? 1 : 0));
    } else if constexpr (Kind == 3) {
        carry = (v & 0x01) != 0;
        res = static_cast<u8>((v >> 1) | (old_carry ? 0x80 : 0));
    } else if constexpr (Kind == 4) {
        carry = (v & 0x80) != 0;
        res = static_cast<u8>(v << 1);
    } else if constexpr (Kind == 5) {
        carry = (v & 0x01) != 0;
        res = static_cast<u8>((v >> 1) | (v & 0x80));
    } else if constexpr (Kind == 6) {
        res = static_cast<u8>((v << 4) | (v >> 4));
    } else {
        carry = (v & 0x01) != 0;
        res = static_cast<u8>(v >> 1);
    }
    u8 f = 0;
    if (with_z && res == 0) {
        f |= kFlagZ;
    }
    if (carry) {
        f |= kFlagC;
    }
    c.r_.f = f;
    return res;
}

u8 Sm83Ops::inc8(Sm83& c, u8 v) {
    const u8 res = static_cast<u8>(v + 1);
    u8 f = static_cast<u8>(c.r_.f & kFlagC);
    if (res == 0) {
        f |= kFlagZ;
    }
    if ((v & 0x0F) == 0x0F) {
        f |= kFlagH;
    }
    c.r_.f = f;
    return res;
}

u8 Sm83Ops::dec8(Sm83& c, u8 v) {
    const u8 res = static_cast<u8>(v - 1);
    u8 f = static_cast<u8>((c.r_.f & kFlagC) | kFlagN);
    if (res == 0) {
        f |= kFlagZ;
    }
    if ((v & 0x0F) == 0x00) {
        f |= kFlagH;
    }
    c.r_.f = f;
    return res;
}

void Sm83Ops::daa(Sm83& c) {
    u8 adjust = 0;
    if ((c.r_.f & kFlagC) != 0) {
        adjust = 0x60;
    }
    if ((c.r_.f & kFlagH) != 0) {
        adjust = static_cast<u8>(adjust | 0x06);
    }
    if ((c.r_.f & kFlagN) == 0) {
        if ((c.r_.a & 0x0F) > 0x09) {
            adjust = static_cast<u8>(adjust | 0x06);
        }
        if (c.r_.a > 0x99) {
            adjust = static_cast<u8>(adjust | 0x60);
        }
        c.r_.a = static_cast<u8>(c.r_.a + adjust);
    } else {
        c.r_.a = static_cast<u8>(c.r_.a - adjust);
    }
    u8 f = static_cast<u8>(c.r_.f & kFlagN);
    if (c.r_.a == 0) {
        f |= kFlagZ;
    }
    if ((adjust & 0x60) != 0) {
        f |= kFlagC;
    }
    c.r_.f = f;
}

void Sm83Ops::add_sp_flags(Sm83& c, u8 imm) {
    u8 f = 0;
    if ((c.r_.sp & 0x0F) + (imm & 0x0F) > 0x0F) {
        f |= kFlagH;
    }
    if ((c.r_.sp & 0xFF) + imm > 0xFF) {
        f |= kFlagC;
    }
    c.r_.f = f;
}

void Sm83Ops::halt(Sm83& c) {
    if (!c.ime_ && c.bus_->pending_irqs() != 0) {
        c.halt_bug_ = true;
    } else {
        c.halted_ = true;
    }
}

template <u8 Op>
void Sm83Ops::exec([[maybe_unused]] Sm83& c) {
    if constexpr (Op == 0x76) {
        halt(c);
    } else if constexpr (Op >= 0x40 && Op <= 0x7F) {
        constexpr u8 kDst = static_cast<u8>((Op >> 3) & 0x07);
        constexpr u8 kSrc = static_cast<u8>(Op & 0x07);
        set_r<kDst>(c, get_r<kSrc>(c));
    } else if constexpr (Op >= 0x80 && Op <= 0xBF) {
        constexpr u8 kKind = static_cast<u8>((Op >> 3) & 0x07);
        constexpr u8 kSrc = static_cast<u8>(Op & 0x07);
        alu<kKind>(c, get_r<kSrc>(c));
    } else if constexpr (Op < 0x40) {
        constexpr u8 kLo = static_cast<u8>(Op & 0x07);
        constexpr u8 kHi = static_cast<u8>((Op >> 3) & 0x07);
        constexpr u8 kPair = static_cast<u8>(kHi >> 1);
        if constexpr (kLo == 0) {
            if constexpr (Op == 0x00) {
                return;
            } else if constexpr (Op == 0x08) {
                const u16 addr = c.fetch16();
                c.bus_->write(addr, static_cast<u8>(c.r_.sp & 0xFF));
                c.bus_->write(static_cast<u16>(addr + 1), static_cast<u8>(c.r_.sp >> 8));
            } else if constexpr (Op == 0x10) {
                c.r_.pc = static_cast<u16>(c.r_.pc + 1);
                c.stopped_ = true;
            } else if constexpr (Op == 0x18) {
                const i8 offset = static_cast<i8>(c.fetch8());
                c.bus_->tick(4);
                c.r_.pc = static_cast<u16>(c.r_.pc + static_cast<u16>(offset));
            } else {
                constexpr u8 kCc = static_cast<u8>(kHi - 4);
                const i8 offset = static_cast<i8>(c.fetch8());
                if (cond<kCc>(c)) {
                    c.bus_->tick(4);
                    c.r_.pc = static_cast<u16>(c.r_.pc + static_cast<u16>(offset));
                }
            }
        } else if constexpr (kLo == 1) {
            if constexpr ((kHi & 1) == 0) {
                set_rp<kPair>(c, c.fetch16());
            } else {
                const u16 hl = c.r_.hl();
                const u16 v = get_rp<kPair>(c);
                c.bus_->tick(4);
                u8 f = static_cast<u8>(c.r_.f & kFlagZ);
                if ((hl & 0x0FFF) + (v & 0x0FFF) > 0x0FFF) {
                    f |= kFlagH;
                }
                if (static_cast<u32>(hl) + v > 0xFFFF) {
                    f |= kFlagC;
                }
                c.r_.f = f;
                c.r_.set_hl(static_cast<u16>(hl + v));
            }
        } else if constexpr (kLo == 2) {
            u16 addr = 0;
            if constexpr (kPair == 0) {
                addr = c.r_.bc();
            } else if constexpr (kPair == 1) {
                addr = c.r_.de();
            } else {
                addr = c.r_.hl();
            }
            if constexpr (kPair == 2) {
                c.r_.set_hl(static_cast<u16>(addr + 1));
            } else if constexpr (kPair == 3) {
                c.r_.set_hl(static_cast<u16>(addr - 1));
            }
            if constexpr ((kHi & 1) == 0) {
                c.bus_->write(addr, c.r_.a);
            } else if constexpr (kPair >= 2) {
                // LD A,(HL+) and LD A,(HL-) run the increment unit over the
                // same address in the same M-cycle as the read.
                c.r_.a = c.bus_->read_idu(addr);
            } else {
                c.r_.a = c.bus_->read(addr);
            }
        } else if constexpr (kLo == 3) {
            const u16 v = get_rp<kPair>(c);
            c.bus_->tick(4);
            c.bus_->idu_pulse(v);
            set_rp<kPair>(c, static_cast<u16>((kHi & 1) == 0 ? v + 1 : v - 1));
        } else if constexpr (kLo == 4) {
            set_r<kHi>(c, inc8(c, get_r<kHi>(c)));
        } else if constexpr (kLo == 5) {
            set_r<kHi>(c, dec8(c, get_r<kHi>(c)));
        } else if constexpr (kLo == 6) {
            set_r<kHi>(c, c.fetch8());
        } else if constexpr (kHi < 4) {
            c.r_.a = rotate<kHi>(c, c.r_.a, false);
        } else if constexpr (kHi == 4) {
            daa(c);
        } else if constexpr (kHi == 5) {
            c.r_.a = static_cast<u8>(~c.r_.a);
            c.r_.f = static_cast<u8>(c.r_.f | kFlagN | kFlagH);
        } else if constexpr (kHi == 6) {
            c.r_.f = static_cast<u8>((c.r_.f & kFlagZ) | kFlagC);
        } else {
            c.r_.f = static_cast<u8>((c.r_.f & (kFlagZ | kFlagC)) ^ kFlagC);
        }
    } else {
        constexpr u8 kLo = static_cast<u8>(Op & 0x07);
        constexpr u8 kHi = static_cast<u8>((Op >> 3) & 0x07);
        constexpr u8 kPair = static_cast<u8>(kHi >> 1);
        if constexpr (is_locked_opcode(Op)) {
            // No fetch of a second byte and no PC advance: the part hangs here forever.
            c.r_.pc = static_cast<u16>(c.r_.pc - 1);
        } else if constexpr (kLo == 6) {
            alu<kHi>(c, c.fetch8());
        } else if constexpr (kLo == 7) {
            c.push16(c.r_.pc);
            c.r_.pc = static_cast<u16>(kHi * 8);
        } else if constexpr (Op == 0xC3) {
            const u16 target = c.fetch16();
            c.bus_->tick(4);
            c.r_.pc = target;
        } else if constexpr (Op == 0xCB) {
            c.execute_cb(c.fetch8());
        } else if constexpr (Op == 0xC9 || Op == 0xD9) {
            const u16 target = c.pop16();
            c.bus_->tick(4);
            c.r_.pc = target;
            if constexpr (Op == 0xD9) {
                c.ime_ = true;
                c.ime_pending_ = false;
            }
        } else if constexpr (Op == 0xCD) {
            const u16 target = c.fetch16();
            c.push16(c.r_.pc);
            c.r_.pc = target;
        } else if constexpr (Op == 0xE9) {
            c.r_.pc = c.r_.hl();
        } else if constexpr (Op == 0xF9) {
            c.bus_->tick(4);
            c.r_.sp = c.r_.hl();
        } else if constexpr (Op == 0xE0 || Op == 0xF0) {
            const u16 addr = static_cast<u16>(0xFF00 + c.fetch8());
            if constexpr (Op == 0xE0) {
                c.bus_->write(addr, c.r_.a);
            } else {
                c.r_.a = c.bus_->read(addr);
            }
        } else if constexpr (Op == 0xE2 || Op == 0xF2) {
            const u16 addr = static_cast<u16>(0xFF00 + c.r_.c);
            if constexpr (Op == 0xE2) {
                c.bus_->write(addr, c.r_.a);
            } else {
                c.r_.a = c.bus_->read(addr);
            }
        } else if constexpr (Op == 0xEA || Op == 0xFA) {
            const u16 addr = c.fetch16();
            if constexpr (Op == 0xEA) {
                c.bus_->write(addr, c.r_.a);
            } else {
                c.r_.a = c.bus_->read(addr);
            }
        } else if constexpr (Op == 0xE8) {
            const u8 imm = c.fetch8();
            add_sp_flags(c, imm);
            c.bus_->tick(4);
            c.r_.sp = static_cast<u16>(c.r_.sp + static_cast<u16>(static_cast<i8>(imm)));
            c.bus_->tick(4);
        } else if constexpr (Op == 0xF8) {
            const u8 imm = c.fetch8();
            const u16 sp = c.r_.sp;
            add_sp_flags(c, imm);
            c.bus_->tick(4);
            c.r_.set_hl(static_cast<u16>(sp + static_cast<u16>(static_cast<i8>(imm))));
        } else if constexpr (Op == 0xF3) {
            c.ime_ = false;
            c.ime_pending_ = false;
        } else if constexpr (Op == 0xFB) {
            c.ime_pending_ = true;
        } else if constexpr (kLo == 1) {
            if constexpr (kPair == 3) {
                c.r_.set_af(c.pop16());
            } else {
                set_rp<kPair>(c, c.pop16());
            }
        } else if constexpr (kLo == 5) {
            if constexpr (kPair == 3) {
                c.push16(c.r_.af());
            } else {
                c.push16(get_rp<kPair>(c));
            }
        } else if constexpr (kLo == 0) {
            c.bus_->tick(4);
            if (cond<kHi>(c)) {
                const u16 target = c.pop16();
                c.bus_->tick(4);
                c.r_.pc = target;
            }
        } else if constexpr (kLo == 2) {
            const u16 target = c.fetch16();
            if (cond<kHi>(c)) {
                c.bus_->tick(4);
                c.r_.pc = target;
            }
        } else {
            const u16 target = c.fetch16();
            if (cond<kHi>(c)) {
                c.push16(c.r_.pc);
                c.r_.pc = target;
            }
        }
    }
}

template <u8 Op>
void Sm83Ops::exec_cb(Sm83& c) {
    constexpr u8 kIdx = static_cast<u8>(Op & 0x07);
    constexpr u8 kBit = static_cast<u8>((Op >> 3) & 0x07);
    if constexpr (Op < 0x40) {
        set_r<kIdx>(c, rotate<kBit>(c, get_r<kIdx>(c), true));
    } else if constexpr (Op < 0x80) {
        const u8 v = get_r<kIdx>(c);
        u8 f = static_cast<u8>((c.r_.f & kFlagC) | kFlagH);
        if ((v & (1u << kBit)) == 0) {
            f |= kFlagZ;
        }
        c.r_.f = f;
    } else if constexpr (Op < 0xC0) {
        set_r<kIdx>(c, static_cast<u8>(get_r<kIdx>(c) & ~(1u << kBit)));
    } else {
        set_r<kIdx>(c, static_cast<u8>(get_r<kIdx>(c) | (1u << kBit)));
    }
}

using OpFn = void (*)(Sm83&);

constexpr std::array<OpFn, 256> kMainOps = [] {
    std::array<OpFn, 256> table{};
    [&table]<std::size_t... I>(std::index_sequence<I...>) {
        ((table[I] = &Sm83Ops::exec<static_cast<u8>(I)>), ...);
    }(std::make_index_sequence<256>{});
    return table;
}();

constexpr std::array<OpFn, 256> kCbOps = [] {
    std::array<OpFn, 256> table{};
    [&table]<std::size_t... I>(std::index_sequence<I...>) {
        ((table[I] = &Sm83Ops::exec_cb<static_cast<u8>(I)>), ...);
    }(std::make_index_sequence<256>{});
    return table;
}();

}  // namespace gb
