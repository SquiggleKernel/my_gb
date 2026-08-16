#include "core/cart/header.h"

#include <array>
#include <cstddef>
#include <string>
#include <utility>

namespace gb {

namespace {

struct CartTypeEntry {
    u8 code;
    Mapper mapper;
    bool battery;
    bool rtc;
    bool rumble;
};

constexpr CartTypeEntry kCartTypes[] = {
    {0x00, Mapper::None, false, false, false},
    {0x01, Mapper::Mbc1, false, false, false},
    {0x02, Mapper::Mbc1, false, false, false},
    {0x03, Mapper::Mbc1, true, false, false},
    {0x05, Mapper::Mbc2, false, false, false},
    {0x06, Mapper::Mbc2, true, false, false},
    {0x08, Mapper::None, false, false, false},
    {0x09, Mapper::None, true, false, false},
    {0x0F, Mapper::Mbc3, true, true, false},
    {0x10, Mapper::Mbc3, true, true, false},
    {0x11, Mapper::Mbc3, false, false, false},
    {0x12, Mapper::Mbc3, false, false, false},
    {0x13, Mapper::Mbc3, true, false, false},
    {0x19, Mapper::Mbc5, false, false, false},
    {0x1A, Mapper::Mbc5, false, false, false},
    {0x1B, Mapper::Mbc5, true, false, false},
    {0x1C, Mapper::Mbc5, false, false, true},
    {0x1D, Mapper::Mbc5, false, false, true},
    {0x1E, Mapper::Mbc5, true, false, true},
};

// Code 0x01 was never used on a shipped cartridge; dumps carrying it behave as 2 KiB.
constexpr std::array<u32, 6> kRamSizes = {0, 2048, 8192, 32768, 131072, 65536};

constexpr std::size_t kHeaderEnd = 0x150;

void set_error(std::string* err, std::string msg) {
    if (err != nullptr) {
        *err = std::move(msg);
    }
}

std::string hex_byte(u8 v) {
    constexpr char kDigits[] = "0123456789ABCDEF";
    std::string s = "0x";
    s.push_back(kDigits[static_cast<std::size_t>(v >> 4)]);
    s.push_back(kDigits[static_cast<std::size_t>(v & 0x0F)]);
    return s;
}

}  // namespace

bool parse_header(std::span<const u8> rom, CartHeader& out, std::string* err) {
    out = CartHeader{};
    if (rom.size() < kHeaderEnd) {
        set_error(err, "ROM is too small to hold a cartridge header");
        return false;
    }

    for (std::size_t i = 0x0134; i <= 0x0143; ++i) {
        const u8 c = rom[i];
        if (c < 0x20 || c > 0x7E) {
            break;
        }
        out.title.push_back(static_cast<char>(c));
    }

    out.cart_type = rom[0x0147];
    out.rom_size_code = rom[0x0148];
    out.ram_size_code = rom[0x0149];

    u8 sum = 0;
    for (std::size_t addr = 0x0134; addr <= 0x014C; ++addr) {
        sum = static_cast<u8>(sum - rom[addr] - 1);
    }
    out.header_checksum_ok = sum == rom[0x014D];

    const CartTypeEntry* entry = nullptr;
    for (const CartTypeEntry& e : kCartTypes) {
        if (e.code == out.cart_type) {
            entry = &e;
            break;
        }
    }
    if (entry == nullptr) {
        out.mapper = Mapper::Unsupported;
        set_error(err, "unsupported cartridge type " + hex_byte(out.cart_type));
        return false;
    }

    out.mapper = entry->mapper;
    out.has_battery = entry->battery;
    out.has_rtc = entry->rtc;
    out.has_rumble = entry->rumble;

    if (out.rom_size_code > 0x08) {
        set_error(err, "unsupported ROM size code " + hex_byte(out.rom_size_code));
        return false;
    }
    out.rom_bytes = static_cast<u32>(32768u << out.rom_size_code);

    // MBC2 keeps its 512 nibbles on the mapper die, so the RAM size code says nothing.
    if (out.mapper == Mapper::Mbc2) {
        out.ram_bytes = 512;
        return true;
    }

    if (out.ram_size_code >= kRamSizes.size()) {
        set_error(err, "unsupported RAM size code " + hex_byte(out.ram_size_code));
        return false;
    }
    out.ram_bytes = kRamSizes[static_cast<std::size_t>(out.ram_size_code)];
    return true;
}

const char* mapper_name(Mapper m) {
    switch (m) {
        case Mapper::None: return "ROM only";
        case Mapper::Mbc1: return "MBC1";
        case Mapper::Mbc2: return "MBC2";
        case Mapper::Mbc3: return "MBC3";
        case Mapper::Mbc5: return "MBC5";
        case Mapper::Unsupported: break;
    }
    return "unsupported";
}

}  // namespace gb
