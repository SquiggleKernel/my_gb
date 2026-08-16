#include "core/cpu/disasm.h"

#include "core/bus.h"

namespace gb {
namespace {

const char* const kR8[8] = {"B", "C", "D", "E", "H", "L", "(HL)", "A"};
const char* const kRp[4] = {"BC", "DE", "HL", "SP"};
const char* const kRp2[4] = {"BC", "DE", "HL", "AF"};
const char* const kInd[4] = {"(BC)", "(DE)", "(HL+)", "(HL-)"};
const char* const kCc[4] = {"NZ", "Z", "NC", "C"};
const char* const kMisc[8] = {"RLCA", "RRCA", "RLA", "RRA", "DAA", "CPL", "SCF", "CCF"};
const char* const kRot[8] = {"RLC", "RRC", "RL", "RR", "SLA", "SRA", "SWAP", "SRL"};
const char* const kRst[8] = {"00H", "08H", "10H", "18H", "20H", "28H", "30H", "38H"};
const char* const kAlu[8] = {"ADD A,", "ADC A,", "SUB ", "SBC A,", "AND ", "XOR ", "OR ", "CP "};

std::string hex(u32 value, int digits) {
    static const char kDigits[] = "0123456789ABCDEF";
    std::string out("$");
    for (int i = digits - 1; i >= 0; --i) {
        out.push_back(kDigits[(value >> (4 * i)) & 0xF]);
    }
    return out;
}

bool locked(u8 op) {
    return op == 0xD3 || op == 0xDB || op == 0xDD || op == 0xE3 || op == 0xE4 || op == 0xEB ||
           op == 0xEC || op == 0xED || op == 0xF4 || op == 0xFC || op == 0xFD;
}

}  // namespace

Disasm disassemble(const Bus& bus, u16 addr) {
    const u8 op = bus.peek(addr);
    const u8 lo = static_cast<u8>(op & 0x07);
    const u8 hi = static_cast<u8>((op >> 3) & 0x07);
    const u8 imm8 = bus.peek(static_cast<u16>(addr + 1));
    const u16 imm16 = static_cast<u16>(imm8 | (bus.peek(static_cast<u16>(addr + 2)) << 8));
    const u16 target = static_cast<u16>(addr + 2 + static_cast<u16>(static_cast<i8>(imm8)));

    Disasm out{addr, 1, {}};

    if (op == 0xCB) {
        const u8 cb_lo = static_cast<u8>(imm8 & 0x07);
        const u8 cb_hi = static_cast<u8>((imm8 >> 3) & 0x07);
        out.length = 2;
        if (imm8 < 0x40) {
            out.text = std::string(kRot[cb_hi]) + " " + kR8[cb_lo];
        } else {
            const char* group = "SET ";
            if (imm8 < 0x80) {
                group = "BIT ";
            } else if (imm8 < 0xC0) {
                group = "RES ";
            }
            out.text = std::string(group) + static_cast<char>('0' + cb_hi) + "," + kR8[cb_lo];
        }
        return out;
    }
    if (locked(op)) {
        out.text = "DB " + hex(op, 2);
        return out;
    }
    if (op == 0x76) {
        out.text = "HALT";
        return out;
    }
    if (op >= 0x40 && op < 0x80) {
        out.text = std::string("LD ") + kR8[hi] + "," + kR8[lo];
        return out;
    }
    if (op >= 0x80 && op < 0xC0) {
        out.text = std::string(kAlu[hi]) + kR8[lo];
        return out;
    }

    if (op < 0x40) {
        switch (lo) {
            case 0:
                if (op == 0x00) {
                    out.text = "NOP";
                } else if (op == 0x08) {
                    out.length = 3;
                    out.text = "LD (" + hex(imm16, 4) + "),SP";
                } else if (op == 0x10) {
                    out.length = 2;
                    out.text = "STOP";
                } else if (op == 0x18) {
                    out.length = 2;
                    out.text = "JR " + hex(target, 4);
                } else {
                    out.length = 2;
                    out.text = std::string("JR ") + kCc[hi - 4] + "," + hex(target, 4);
                }
                break;
            case 1:
                if ((hi & 1) == 0) {
                    out.length = 3;
                    out.text = std::string("LD ") + kRp[hi >> 1] + "," + hex(imm16, 4);
                } else {
                    out.text = std::string("ADD HL,") + kRp[hi >> 1];
                }
                break;
            case 2:
                if ((hi & 1) == 0) {
                    out.text = std::string("LD ") + kInd[hi >> 1] + ",A";
                } else {
                    out.text = std::string("LD A,") + kInd[hi >> 1];
                }
                break;
            case 3:
                out.text = std::string((hi & 1) == 0 ? "INC " : "DEC ") + kRp[hi >> 1];
                break;
            case 4:
                out.text = std::string("INC ") + kR8[hi];
                break;
            case 5:
                out.text = std::string("DEC ") + kR8[hi];
                break;
            case 6:
                out.length = 2;
                out.text = std::string("LD ") + kR8[hi] + "," + hex(imm8, 2);
                break;
            default:
                out.text = kMisc[hi];
                break;
        }
        return out;
    }

    switch (lo) {
        case 0:
            if (hi < 4) {
                out.text = std::string("RET ") + kCc[hi];
            } else if (op == 0xE0) {
                out.length = 2;
                out.text = "LDH (" + hex(imm8, 2) + "),A";
            } else if (op == 0xE8) {
                out.length = 2;
                out.text = "ADD SP," + hex(imm8, 2);
            } else if (op == 0xF0) {
                out.length = 2;
                out.text = "LDH A,(" + hex(imm8, 2) + ")";
            } else {
                out.length = 2;
                out.text = "LD HL,SP+" + hex(imm8, 2);
            }
            break;
        case 1:
            if ((hi & 1) == 0) {
                out.text = std::string("POP ") + kRp2[hi >> 1];
            } else if (op == 0xC9) {
                out.text = "RET";
            } else if (op == 0xD9) {
                out.text = "RETI";
            } else if (op == 0xE9) {
                out.text = "JP (HL)";
            } else {
                out.text = "LD SP,HL";
            }
            break;
        case 2:
            if (hi < 4) {
                out.length = 3;
                out.text = std::string("JP ") + kCc[hi] + "," + hex(imm16, 4);
            } else if (op == 0xE2) {
                out.text = "LD (C),A";
            } else if (op == 0xF2) {
                out.text = "LD A,(C)";
            } else if (op == 0xEA) {
                out.length = 3;
                out.text = "LD (" + hex(imm16, 4) + "),A";
            } else {
                out.length = 3;
                out.text = "LD A,(" + hex(imm16, 4) + ")";
            }
            break;
        case 3:
            if (op == 0xC3) {
                out.length = 3;
                out.text = "JP " + hex(imm16, 4);
            } else if (op == 0xF3) {
                out.text = "DI";
            } else {
                out.text = "EI";
            }
            break;
        case 4:
            out.length = 3;
            out.text = std::string("CALL ") + kCc[hi] + "," + hex(imm16, 4);
            break;
        case 5:
            if ((hi & 1) == 0) {
                out.text = std::string("PUSH ") + kRp2[hi >> 1];
            } else {
                out.length = 3;
                out.text = "CALL " + hex(imm16, 4);
            }
            break;
        case 6:
            out.length = 2;
            out.text = std::string(kAlu[hi]) + hex(imm8, 2);
            break;
        default:
            out.text = std::string("RST ") + kRst[hi];
            break;
    }
    return out;
}

}  // namespace gb
