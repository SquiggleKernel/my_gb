#pragma once

#include <cstddef>
#include <vector>

#include "core/cart/cartridge.h"
#include "core/cart/header.h"
#include "core/state/serialize.h"
#include "core/types.h"

namespace gb {

class NoMbc final : public Cartridge {
  public:
    NoMbc(std::vector<u8> rom, const CartHeader& hdr);

    u8 read_rom(u16 addr) const override;
    void write_rom(u16 addr, u8 value) override;
    u8 read_ram(u16 addr) const override;
    void write_ram(u16 addr, u8 value) override;

    void save_state(WriteArchive& ar) override;
    void load_state(ReadArchive& ar) override;

  private:
    template <typename Ar>
    void visit(Ar& ar);
};

class Mbc1 final : public Cartridge {
  public:
    Mbc1(std::vector<u8> rom, const CartHeader& hdr);

    u8 read_rom(u16 addr) const override;
    void write_rom(u16 addr, u8 value) override;
    u8 read_ram(u16 addr) const override;
    void write_ram(u16 addr, u8 value) override;

    void save_state(WriteArchive& ar) override;
    void load_state(ReadArchive& ar) override;

    bool multicart() const { return multicart_; }

  private:
    template <typename Ar>
    void visit(Ar& ar);

    int bank_shift() const;
    std::size_t low_bank() const;
    std::size_t high_bank() const;
    std::size_t ram_bank() const;

    u8 bank_lo_ = 1;
    u8 bank_hi_ = 0;
    bool mode_ = false;
    bool ram_enabled_ = false;
    bool multicart_ = false;
};

class Mbc2 final : public Cartridge {
  public:
    Mbc2(std::vector<u8> rom, const CartHeader& hdr);

    u8 read_rom(u16 addr) const override;
    void write_rom(u16 addr, u8 value) override;
    u8 read_ram(u16 addr) const override;
    void write_ram(u16 addr, u8 value) override;

    void save_state(WriteArchive& ar) override;
    void load_state(ReadArchive& ar) override;

  private:
    template <typename Ar>
    void visit(Ar& ar);

    u8 bank_ = 1;
    bool ram_enabled_ = false;
};

struct RtcRegs {
    u8 seconds = 0;
    u8 minutes = 0;
    u8 hours = 0;
    u8 day_low = 0;
    u8 day_high = 0;
};

class Mbc3 final : public Cartridge {
  public:
    Mbc3(std::vector<u8> rom, const CartHeader& hdr);

    u8 read_rom(u16 addr) const override;
    void write_rom(u16 addr, u8 value) override;
    u8 read_ram(u16 addr) const override;
    void write_ram(u16 addr, u8 value) override;

    void tick(u64 tcycles) override;

    void save_state(WriteArchive& ar) override;
    void load_state(ReadArchive& ar) override;

  private:
    template <typename Ar>
    void visit(Ar& ar);

    void advance_second();

    u8 rom_bank_ = 1;
    u8 select_ = 0;
    u8 latch_ = 0xFF;
    bool ram_enabled_ = false;
    u64 rtc_sub_ = 0;
    RtcRegs rtc_{};
    RtcRegs latched_{};
};

class Mbc5 final : public Cartridge {
  public:
    Mbc5(std::vector<u8> rom, const CartHeader& hdr);

    u8 read_rom(u16 addr) const override;
    void write_rom(u16 addr, u8 value) override;
    u8 read_ram(u16 addr) const override;
    void write_ram(u16 addr, u8 value) override;

    void save_state(WriteArchive& ar) override;
    void load_state(ReadArchive& ar) override;

  private:
    template <typename Ar>
    void visit(Ar& ar);

    u16 rom_bank_ = 1;
    u8 ram_bank_ = 0;
    bool ram_enabled_ = false;
};

}  // namespace gb
