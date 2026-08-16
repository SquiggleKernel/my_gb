#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "core/cart/cartridge.h"
#include "core/cart/header.h"

namespace {

// Each 16 KiB bank is stamped with its own index at its first byte, so a read
// from 0x4000 tells you exactly which bank is mapped.
std::vector<gb::u8> build_rom(gb::u8 cart_type, gb::u8 rom_code, gb::u8 ram_code) {
    const std::size_t size = static_cast<std::size_t>(32768) << rom_code;
    std::vector<gb::u8> rom(size, 0x00);
    for (std::size_t bank = 0; bank * 0x4000 < size; ++bank) {
        rom[bank * 0x4000] = static_cast<gb::u8>(bank);
    }
    rom[0x147] = cart_type;
    rom[0x148] = rom_code;
    rom[0x149] = ram_code;
    return rom;
}

std::unique_ptr<gb::Cartridge> load(gb::u8 cart_type, gb::u8 rom_code, gb::u8 ram_code) {
    std::string err;
    auto cart = gb::make_cartridge(build_rom(cart_type, rom_code, ram_code), &err);
    REQUIRE(cart != nullptr);
    return cart;
}

}  // namespace

TEST_CASE("a truncated ROM is rejected without reading past the end", "[cart]") {
    std::vector<gb::u8> rom(0x100, 0);
    gb::CartHeader hdr;
    std::string err;
    REQUIRE_FALSE(gb::parse_header(rom, hdr, &err));
    REQUIRE_FALSE(err.empty());
}

TEST_CASE("an unsupported mapper is refused rather than guessed at", "[cart]") {
    std::string err;
    auto cart = gb::make_cartridge(build_rom(0xFE, 0x00, 0x00), &err);
    REQUIRE(cart == nullptr);
    REQUIRE_FALSE(err.empty());
}

TEST_CASE("a ROM-only cart maps both banks statically", "[cart]") {
    auto cart = load(0x00, 0x00, 0x00);
    REQUIRE(cart->header().mapper == gb::Mapper::None);
    REQUIRE(cart->read_rom(0x0000) == 0);
    REQUIRE(cart->read_rom(0x4000) == 1);
}

TEST_CASE("MBC1 switches the upper window and never maps bank 0 there", "[cart]") {
    auto cart = load(0x01, 0x04, 0x00);  // 512 KiB, 32 banks

    cart->write_rom(0x2000, 0x05);
    REQUIRE(cart->read_rom(0x4000) == 5);
    REQUIRE(cart->read_rom(0x0000) == 0);

    // Writing 0 to the bank register selects bank 1, not bank 0.
    cart->write_rom(0x2000, 0x00);
    REQUIRE(cart->read_rom(0x4000) == 1);

    cart->write_rom(0x2000, 0x1F);
    REQUIRE(cart->read_rom(0x4000) == 31);
}

TEST_CASE("MBC1 RAM stays inert until it is enabled", "[cart]") {
    auto cart = load(0x03, 0x04, 0x02);  // battery + 8 KiB RAM

    cart->write_ram(0xA000, 0x5A);
    REQUIRE(cart->read_ram(0xA000) == 0xFF);

    cart->write_rom(0x0000, 0x0A);
    cart->write_ram(0xA000, 0x5A);
    REQUIRE(cart->read_ram(0xA000) == 0x5A);

    cart->write_rom(0x0000, 0x00);
    REQUIRE(cart->read_ram(0xA000) == 0xFF);
}

TEST_CASE("MBC2 has 512 nibbles of internal RAM", "[cart]") {
    auto cart = load(0x05, 0x03, 0x00);
    REQUIRE(cart->header().mapper == gb::Mapper::Mbc2);
    REQUIRE(cart->header().ram_bytes == 512);

    // Bit 8 of the address picks between RAM enable and the bank register.
    cart->write_rom(0x0000, 0x0A);
    cart->write_ram(0xA000, 0x35);
    REQUIRE(cart->read_ram(0xA000) == 0xF5);

    // The 512-nibble array mirrors every 0x200 bytes.
    REQUIRE(cart->read_ram(0xA200) == 0xF5);
}

TEST_CASE("MBC2 selects a ROM bank when address bit 8 is set", "[cart]") {
    auto cart = load(0x05, 0x03, 0x00);
    cart->write_rom(0x0100, 0x03);
    REQUIRE(cart->read_rom(0x4000) == 3);
    cart->write_rom(0x0100, 0x00);
    REQUIRE(cart->read_rom(0x4000) == 1);
}

TEST_CASE("MBC5 is the one mapper that can map bank 0 into the upper window", "[cart]") {
    auto cart = load(0x19, 0x04, 0x00);
    cart->write_rom(0x2000, 0x00);
    REQUIRE(cart->read_rom(0x4000) == 0);

    cart->write_rom(0x2000, 0x11);
    REQUIRE(cart->read_rom(0x4000) == 0x11);
}

TEST_CASE("cart RAM survives a state round trip", "[cart]") {
    auto cart = load(0x03, 0x04, 0x02);
    cart->write_rom(0x0000, 0x0A);
    cart->write_ram(0xA000, 0x77);
    cart->write_ram(0xA001, 0x88);

    std::vector<gb::u8> blob;
    gb::WriteArchive w(blob);
    cart->save_state(w);

    auto restored = load(0x03, 0x04, 0x02);
    gb::ReadArchive r(blob.data(), blob.size());
    restored->load_state(r);
    REQUIRE(r.ok());

    restored->write_rom(0x0000, 0x0A);
    REQUIRE(restored->read_ram(0xA000) == 0x77);
    REQUIRE(restored->read_ram(0xA001) == 0x88);
}
