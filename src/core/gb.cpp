#include "core/gb.h"

#include <utility>

namespace gb {

namespace {

constexpr u32 kStateMagic = 0x3142474D;  // "MGB1"
constexpr u32 kStateVersion = 2;

// Mapper::Unsupported is 5, so this stays clear of every real discriminator.
constexpr u8 kNoCartTag = 0xFF;

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
            break;
        }
    }
    if (rewind_enabled_) {
        rewind_.capture(*this);
    }
}

u8 Gb::cart_tag() const {
    const Cartridge* cart = bus_.cartridge();
    return cart == nullptr ? kNoCartTag : static_cast<u8>(cart->header().mapper);
}

u32 Gb::cart_rom_size() const {
    const Cartridge* cart = bus_.cartridge();
    return cart == nullptr ? 0u : cart->header().rom_bytes;
}

bool Gb::read_header(ReadArchive& ar) const {
    u32 magic = 0;
    u32 version = 0;
    u8 mapper = 0;
    u32 rom_bytes = 0;
    ar(magic);
    ar(version);
    ar(mapper);
    ar(rom_bytes);
    return ar.ok() && magic == kStateMagic && version == kStateVersion &&
           mapper == cart_tag() && rom_bytes == cart_rom_size();
}

void Gb::apply_payload(ReadArchive& ar) {
    cpu_.visit(ar);
    bus_.visit(ar);
    if (Cartridge* cart = bus_.cartridge()) {
        cart->load_state(ar);
    }
}

std::vector<u8> Gb::save_state() {
    std::vector<u8> out;
    write_state(out);
    return out;
}

void Gb::write_state(std::vector<u8>& out) {
    out.clear();
    WriteArchive ar(out);
    u32 magic = kStateMagic;
    u32 version = kStateVersion;
    u8 mapper = cart_tag();
    u32 rom_bytes = cart_rom_size();
    ar(magic);
    ar(version);
    ar(mapper);
    ar(rom_bytes);
    cpu_.visit(ar);
    bus_.visit(ar);
    if (Cartridge* cart = bus_.cartridge()) {
        cart->save_state(ar);
    }
}

bool Gb::load_state(std::span<const u8> data) {
    ReadArchive ar(data.data(), data.size());
    if (!read_header(ar)) {
        return false;
    }

    // The payload is applied in place, so keep a complete copy of the live
    // machine first. Every visit() assigns unconditionally, which is what makes
    // replaying a known-good state enough to undo a half-applied one.
    write_state(rollback_);

    apply_payload(ar);
    if (!ar.ok()) {
        ReadArchive undo(rollback_.data(), rollback_.size());
        read_header(undo);  // only here to step the cursor past the header
        apply_payload(undo);
        return false;
    }
    return true;
}

}  // namespace gb
