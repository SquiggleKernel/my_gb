#include "core/cart/mbc.h"

#include <algorithm>
#include <array>
#include <utility>

namespace gb {

namespace {

constexpr std::size_t kRomBankSize = 0x4000;
constexpr std::size_t kRamBankSize = 0x2000;
constexpr std::size_t kMulticartSize = 0x100000;
constexpr std::size_t kMulticartStride = 0x40000;

constexpr std::array<u8, 48> kNintendoLogo = {
    0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83,
    0x00, 0x0C, 0x00, 0x0D, 0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
    0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99, 0xBB, 0xBB, 0x67, 0x63,
    0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E,
};

u8 rom_byte(const std::vector<u8>& rom, std::size_t bank, u16 addr) {
    const std::size_t banks = rom.size() / kRomBankSize;
    if (banks == 0) {
        return 0xFF;
    }
    const std::size_t off =
        (bank % banks) * kRomBankSize + (static_cast<std::size_t>(addr) & 0x3FFF);
    return off < rom.size() ? rom[off] : 0xFF;
}

// A RAM chip smaller than the window decodes fewer address lines, so short RAM mirrors
// instead of reading open bus.
std::size_t ram_offset(const std::vector<u8>& ram, std::size_t bank, u16 addr) {
    const std::size_t off = bank * kRamBankSize + (static_cast<std::size_t>(addr) & 0x1FFF);
    return off % ram.size();
}

u8 ram_byte(const std::vector<u8>& ram, std::size_t bank, u16 addr) {
    if (ram.empty()) {
        return 0xFF;
    }
    return ram[ram_offset(ram, bank, addr)];
}

void store_ram_byte(std::vector<u8>& ram, std::size_t bank, u16 addr, u8 value) {
    if (ram.empty()) {
        return;
    }
    ram[ram_offset(ram, bank, addr)] = value;
}

bool has_logo_at(const std::vector<u8>& rom, std::size_t base) {
    const std::size_t off = base + 0x0104;
    if (off + kNintendoLogo.size() > rom.size()) {
        return false;
    }
    return std::equal(kNintendoLogo.begin(), kNintendoLogo.end(), rom.data() + off);
}

// Multicart compilations wire one fewer bank line and stack several games, each with
// its own boot logo, at quarter-megabyte boundaries.
bool looks_like_multicart(const std::vector<u8>& rom) {
    if (rom.size() != kMulticartSize) {
        return false;
    }
    int matches = 0;
    for (std::size_t base = 0; base < rom.size(); base += kMulticartStride) {
        if (has_logo_at(rom, base)) {
            ++matches;
        }
    }
    return matches >= 3;
}

template <typename Ar>
void archive_ram(Ar& ar, std::vector<u8>& ram) {
    if (!ram.empty()) {
        ar.bytes(ram.data(), ram.size());
    }
}

template <typename Ar>
void archive_rtc(Ar& ar, RtcRegs& r) {
    ar(r.seconds);
    ar(r.minutes);
    ar(r.hours);
    ar(r.day_low);
    ar(r.day_high);
}

u8 rtc_read(const RtcRegs& r, u8 sel) {
    switch (sel) {
        case 0x08: return r.seconds;
        case 0x09: return r.minutes;
        case 0x0A: return r.hours;
        case 0x0B: return r.day_low;
        case 0x0C: return r.day_high;
        default: return 0xFF;
    }
}

void rtc_write(RtcRegs& r, u8 sel, u8 value) {
    switch (sel) {
        case 0x08: r.seconds = static_cast<u8>(value & 0x3F); break;
        case 0x09: r.minutes = static_cast<u8>(value & 0x3F); break;
        case 0x0A: r.hours = static_cast<u8>(value & 0x1F); break;
        case 0x0B: r.day_low = value; break;
        case 0x0C: r.day_high = static_cast<u8>(value & 0xC1); break;
        default: break;
    }
}

}  // namespace

NoMbc::NoMbc(std::vector<u8> rom, const CartHeader& hdr) : Cartridge(std::move(rom), hdr) {}

u8 NoMbc::read_rom(u16 addr) const {
    const std::size_t off = static_cast<std::size_t>(addr);
    return off < rom_.size() ? rom_[off] : 0xFF;
}

void NoMbc::write_rom(u16, u8) {}

u8 NoMbc::read_ram(u16 addr) const { return ram_byte(ram_, std::size_t{0}, addr); }

void NoMbc::write_ram(u16 addr, u8 value) { store_ram_byte(ram_, std::size_t{0}, addr, value); }

template <typename Ar>
void NoMbc::visit(Ar& ar) {
    archive_ram(ar, ram_);
}

void NoMbc::save_state(WriteArchive& ar) { visit(ar); }

void NoMbc::load_state(ReadArchive& ar) { visit(ar); }

Mbc1::Mbc1(std::vector<u8> rom, const CartHeader& hdr) : Cartridge(std::move(rom), hdr) {
    multicart_ = looks_like_multicart(rom_);
}

int Mbc1::bank_shift() const { return multicart_ ? 4 : 5; }

std::size_t Mbc1::low_bank() const {
    if (!mode_) {
        return 0;
    }
    return static_cast<std::size_t>(bank_hi_) << bank_shift();
}

std::size_t Mbc1::high_bank() const {
    const u8 mask = static_cast<u8>(multicart_ ? 0x0F : 0x1F);
    const std::size_t low = static_cast<std::size_t>(bank_lo_ & mask);
    return (static_cast<std::size_t>(bank_hi_) << bank_shift()) | low;
}

std::size_t Mbc1::ram_bank() const { return mode_ ? static_cast<std::size_t>(bank_hi_) : 0; }

u8 Mbc1::read_rom(u16 addr) const {
    return rom_byte(rom_, addr < 0x4000 ? low_bank() : high_bank(), addr);
}

void Mbc1::write_rom(u16 addr, u8 value) {
    if (addr < 0x2000) {
        ram_enabled_ = (value & 0x0F) == 0x0A;
    } else if (addr < 0x4000) {
        bank_lo_ = static_cast<u8>(value & 0x1F);
        // A written zero comes out as one, which is why bank 0 is unreachable at 0x4000.
        if (bank_lo_ == 0) {
            bank_lo_ = 1;
        }
    } else if (addr < 0x6000) {
        bank_hi_ = static_cast<u8>(value & 0x03);
    } else {
        mode_ = (value & 0x01) != 0;
    }
}

u8 Mbc1::read_ram(u16 addr) const {
    if (!ram_enabled_) {
        return 0xFF;
    }
    return ram_byte(ram_, ram_bank(), addr);
}

void Mbc1::write_ram(u16 addr, u8 value) {
    if (!ram_enabled_) {
        return;
    }
    store_ram_byte(ram_, ram_bank(), addr, value);
}

template <typename Ar>
void Mbc1::visit(Ar& ar) {
    archive_ram(ar, ram_);
    ar(bank_lo_);
    ar(bank_hi_);
    ar(mode_);
    ar(ram_enabled_);
}

void Mbc1::save_state(WriteArchive& ar) { visit(ar); }

void Mbc1::load_state(ReadArchive& ar) { visit(ar); }

Mbc2::Mbc2(std::vector<u8> rom, const CartHeader& hdr) : Cartridge(std::move(rom), hdr) {}

u8 Mbc2::read_rom(u16 addr) const {
    const std::size_t bank = addr < 0x4000 ? std::size_t{0} : static_cast<std::size_t>(bank_);
    return rom_byte(rom_, bank, addr);
}

void Mbc2::write_rom(u16 addr, u8 value) {
    if (addr >= 0x4000) {
        return;
    }
    // MBC2 has no separate register windows; address bit 8 picks which register is hit.
    if ((addr & 0x0100) == 0) {
        ram_enabled_ = (value & 0x0F) == 0x0A;
        return;
    }
    bank_ = static_cast<u8>(value & 0x0F);
    if (bank_ == 0) {
        bank_ = 1;
    }
}

u8 Mbc2::read_ram(u16 addr) const {
    if (!ram_enabled_ || ram_.empty()) {
        return 0xFF;
    }
    // Only four data lines reach the internal RAM, so the top nibble reads back as ones.
    return static_cast<u8>(ram_[static_cast<std::size_t>(addr) & 0x01FF] | 0xF0);
}

void Mbc2::write_ram(u16 addr, u8 value) {
    if (!ram_enabled_ || ram_.empty()) {
        return;
    }
    ram_[static_cast<std::size_t>(addr) & 0x01FF] = static_cast<u8>(value & 0x0F);
}

template <typename Ar>
void Mbc2::visit(Ar& ar) {
    archive_ram(ar, ram_);
    ar(bank_);
    ar(ram_enabled_);
}

void Mbc2::save_state(WriteArchive& ar) { visit(ar); }

void Mbc2::load_state(ReadArchive& ar) { visit(ar); }

Mbc3::Mbc3(std::vector<u8> rom, const CartHeader& hdr) : Cartridge(std::move(rom), hdr) {}

u8 Mbc3::read_rom(u16 addr) const {
    const std::size_t bank = addr < 0x4000 ? std::size_t{0} : static_cast<std::size_t>(rom_bank_);
    return rom_byte(rom_, bank, addr);
}

void Mbc3::write_rom(u16 addr, u8 value) {
    if (addr < 0x2000) {
        ram_enabled_ = (value & 0x0F) == 0x0A;
    } else if (addr < 0x4000) {
        rom_bank_ = static_cast<u8>(value & 0x7F);
        if (rom_bank_ == 0) {
            rom_bank_ = 1;
        }
    } else if (addr < 0x6000) {
        select_ = value;
    } else {
        if (latch_ == 0x00 && value == 0x01) {
            latched_ = rtc_;
        }
        latch_ = value;
    }
}

u8 Mbc3::read_ram(u16 addr) const {
    if (!ram_enabled_) {
        return 0xFF;
    }
    if (select_ <= 0x03) {
        return ram_byte(ram_, static_cast<std::size_t>(select_), addr);
    }
    // The guest sees the latched copy; the counter underneath keeps running.
    if (select_ >= 0x08 && select_ <= 0x0C) {
        return rtc_read(latched_, select_);
    }
    return 0xFF;
}

void Mbc3::write_ram(u16 addr, u8 value) {
    if (!ram_enabled_) {
        return;
    }
    if (select_ <= 0x03) {
        store_ram_byte(ram_, static_cast<std::size_t>(select_), addr, value);
        return;
    }
    if (select_ >= 0x08 && select_ <= 0x0C) {
        rtc_write(rtc_, select_, value);
        // Storing seconds also clears the sub-second divider.
        if (select_ == 0x08) {
            rtc_sub_ = 0;
        }
    }
}

// Driven from emulated cycles rather than the host clock, so a run replays identically.
void Mbc3::tick(u64 tcycles) {
    if (!hdr_.has_rtc || (rtc_.day_high & 0x40) != 0) {
        return;
    }
    rtc_sub_ += tcycles;
    while (rtc_sub_ >= kTCyclesPerSecond) {
        rtc_sub_ -= kTCyclesPerSecond;
        advance_second();
    }
}

void Mbc3::advance_second() {
    rtc_.seconds = static_cast<u8>(rtc_.seconds + 1);
    if (rtc_.seconds < 60) {
        return;
    }
    rtc_.seconds = 0;
    rtc_.minutes = static_cast<u8>(rtc_.minutes + 1);
    if (rtc_.minutes < 60) {
        return;
    }
    rtc_.minutes = 0;
    rtc_.hours = static_cast<u8>(rtc_.hours + 1);
    if (rtc_.hours < 24) {
        return;
    }
    rtc_.hours = 0;

    u16 day = static_cast<u16>(rtc_.day_low | (static_cast<u16>(rtc_.day_high & 0x01) << 8));
    day = static_cast<u16>(day + 1);
    if (day > 511) {
        day = 0;
        rtc_.day_high = static_cast<u8>(rtc_.day_high | 0x80);
    }
    rtc_.day_low = static_cast<u8>(day & 0xFF);
    rtc_.day_high = static_cast<u8>((rtc_.day_high & 0xFE) | ((day >> 8) & 0x01));
}

template <typename Ar>
void Mbc3::visit(Ar& ar) {
    archive_ram(ar, ram_);
    ar(rom_bank_);
    ar(select_);
    ar(latch_);
    ar(ram_enabled_);
    ar(rtc_sub_);
    archive_rtc(ar, rtc_);
    archive_rtc(ar, latched_);
}

void Mbc3::save_state(WriteArchive& ar) { visit(ar); }

void Mbc3::load_state(ReadArchive& ar) { visit(ar); }

Mbc5::Mbc5(std::vector<u8> rom, const CartHeader& hdr) : Cartridge(std::move(rom), hdr) {}

u8 Mbc5::read_rom(u16 addr) const {
    const std::size_t bank = addr < 0x4000 ? std::size_t{0} : static_cast<std::size_t>(rom_bank_);
    return rom_byte(rom_, bank, addr);
}

void Mbc5::write_rom(u16 addr, u8 value) {
    if (addr < 0x2000) {
        ram_enabled_ = (value & 0x0F) == 0x0A;
    } else if (addr < 0x3000) {
        // MBC5 is the only mapper that can map bank 0 into the switchable window.
        rom_bank_ = static_cast<u16>((rom_bank_ & 0x0100) | value);
    } else if (addr < 0x4000) {
        rom_bank_ = static_cast<u16>((rom_bank_ & 0x00FF) | (static_cast<u16>(value & 0x01) << 8));
    } else if (addr < 0x6000) {
        ram_bank_ = static_cast<u8>(value & 0x0F);
    }
}

u8 Mbc5::read_ram(u16 addr) const {
    if (!ram_enabled_) {
        return 0xFF;
    }
    return ram_byte(ram_, static_cast<std::size_t>(ram_bank_), addr);
}

void Mbc5::write_ram(u16 addr, u8 value) {
    if (!ram_enabled_) {
        return;
    }
    store_ram_byte(ram_, static_cast<std::size_t>(ram_bank_), addr, value);
}

template <typename Ar>
void Mbc5::visit(Ar& ar) {
    archive_ram(ar, ram_);
    ar(rom_bank_);
    ar(ram_bank_);
    ar(ram_enabled_);
}

void Mbc5::save_state(WriteArchive& ar) { visit(ar); }

void Mbc5::load_state(ReadArchive& ar) { visit(ar); }

}  // namespace gb
