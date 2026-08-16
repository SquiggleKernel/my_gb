#include "core/cart/cartridge.h"

#include <algorithm>
#include <cstddef>
#include <new>
#include <utility>

#include "core/cart/mbc.h"

namespace gb {

Cartridge::Cartridge(std::vector<u8> rom, const CartHeader& hdr)
    : rom_(std::move(rom)), ram_(static_cast<std::size_t>(hdr.ram_bytes), u8{0}), hdr_(hdr) {}

void Cartridge::restore_ram(std::span<const u8> data) {
    const std::size_t n = std::min(ram_.size(), data.size());
    if (n == 0) {
        return;
    }
    std::copy_n(data.begin(), n, ram_.begin());
}

std::unique_ptr<Cartridge> make_cartridge(std::vector<u8> rom, std::string* err) {
    CartHeader hdr;
    if (!parse_header(rom, hdr, err)) {
        return nullptr;
    }

    try {
        // Trimmed dumps are common in test suites; the missing tail reads as open bus.
        if (rom.size() < hdr.rom_bytes) {
            rom.resize(static_cast<std::size_t>(hdr.rom_bytes), u8{0xFF});
        }

        switch (hdr.mapper) {
            case Mapper::None:
                return std::make_unique<NoMbc>(std::move(rom), hdr);
            case Mapper::Mbc1:
                return std::make_unique<Mbc1>(std::move(rom), hdr);
            case Mapper::Mbc2:
                return std::make_unique<Mbc2>(std::move(rom), hdr);
            case Mapper::Mbc3:
                return std::make_unique<Mbc3>(std::move(rom), hdr);
            case Mapper::Mbc5:
                return std::make_unique<Mbc5>(std::move(rom), hdr);
            case Mapper::Unsupported:
                break;
        }
    } catch (const std::bad_alloc&) {
        if (err != nullptr) {
            *err = "out of memory";
        }
        return nullptr;
    }

    if (err != nullptr) {
        *err = "unsupported mapper";
    }
    return nullptr;
}

}  // namespace gb
