#pragma once

#include <memory>
#include <span>
#include <string>
#include <vector>

#include "core/cart/header.h"
#include "core/state/serialize.h"
#include "core/types.h"

namespace gb {

class Cartridge {
  public:
    virtual ~Cartridge() = default;

    virtual u8 read_rom(u16 addr) const = 0;
    virtual void write_rom(u16 addr, u8 value) = 0;
    virtual u8 read_ram(u16 addr) const = 0;
    virtual void write_ram(u16 addr, u8 value) = 0;

    // Only MBC3 with an RTC needs this.
    virtual void tick(u64 tcycles) { (void)tcycles; }

    // visit() cannot be a virtual template, so the two archives are spelled out.
    virtual void save_state(WriteArchive& ar) { (void)ar; }
    virtual void load_state(ReadArchive& ar) { (void)ar; }

    const CartHeader& header() const { return hdr_; }
    std::span<const u8> ram() const { return ram_; }
    bool has_battery() const { return hdr_.has_battery; }
    void restore_ram(std::span<const u8> data);

  protected:
    Cartridge(std::vector<u8> rom, const CartHeader& hdr);

    std::vector<u8> rom_;
    std::vector<u8> ram_;
    CartHeader hdr_;
};

// Returns nullptr and fills `err` on a malformed or unsupported ROM.
std::unique_ptr<Cartridge> make_cartridge(std::vector<u8> rom, std::string* err);

}  // namespace gb
