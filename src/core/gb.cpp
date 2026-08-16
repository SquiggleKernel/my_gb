#include "core/gb.h"

#include <utility>

namespace gb {

namespace {

constexpr u32 kStateMagic = 0x3142474D;  // "MGB1"
constexpr u32 kStateVersion = 1;

}  // namespace

Gb::Gb() {
    cpu_.attach(&bus_);
    reset();
}

bool Gb::load_rom(std::vector<u8> rom, std::string* err) {
    auto cart = make_cartridge(std::move(rom), err);
    if (!cart) {
        return false;
    }
    bus_.load_cartridge(std::move(cart));
    reset();
    return true;
}

void Gb::reset() {
    bus_.reset();
    cpu_.reset();
}

void Gb::step_instruction() { cpu_.step(); }

void Gb::run_cycles(u64 tcycles) {
    const u64 target = bus_.cycles() + tcycles;
    while (bus_.cycles() < target) {
        cpu_.step();
    }
}

void Gb::run_frame() {
    // With the LCD off no frame is ever produced, so cap the run instead of
    // spinning forever.
    const u64 cap = bus_.cycles() + kTCyclesPerFrame * 2;
    while (bus_.cycles() < cap) {
        cpu_.step();
        if (bus_.ppu().take_frame()) {
            return;
        }
    }
}

std::vector<u8> Gb::save_state() {
    std::vector<u8> out;
    WriteArchive ar(out);
    u32 magic = kStateMagic;
    u32 version = kStateVersion;
    ar(magic);
    ar(version);
    cpu_.visit(ar);
    bus_.visit(ar);
    if (Cartridge* cart = bus_.cartridge()) {
        cart->save_state(ar);
    }
    return out;
}

bool Gb::load_state(std::span<const u8> data) {
    ReadArchive ar(data.data(), data.size());
    u32 magic = 0;
    u32 version = 0;
    ar(magic);
    ar(version);
    if (!ar.ok() || magic != kStateMagic || version != kStateVersion) {
        return false;
    }
    cpu_.visit(ar);
    bus_.visit(ar);
    if (Cartridge* cart = bus_.cartridge()) {
        cart->load_state(ar);
    }
    return ar.ok();
}

}  // namespace gb
