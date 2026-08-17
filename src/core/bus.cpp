#include "core/bus.h"

#include <utility>

namespace gb {

Bus::Bus() {
    timer_.attach(this);
    ppu_.attach(this);
    apu_.attach(this);
    serial_.attach(this);
    joypad_.attach(this);
    reset();
}

void Bus::reset() {
    sched_.reset();
    timer_.reset();
    ppu_.reset();
    apu_.reset();
    serial_.reset();
    joypad_.reset();
    wram_.fill(0);
    hram_.fill(0);
    if_ = 0xE1;
    ie_ = 0;
    dma_page_ = 0xFF;
    dma_index_ = 0;
    dma_delay_ = 0;
    dma_active_ = false;
    dma_latch_ = 0xFF;
    dma_pending_page_ = 0xFF;
    dma_start_delay_ = 0;
}

void Bus::load_cartridge(std::unique_ptr<Cartridge> cart) { cart_ = std::move(cart); }

void Bus::tick(u64 tcycles) {
    sched_.advance(tcycles);
    timer_.tick(tcycles);
    ppu_.tick(tcycles);
    apu_.tick(tcycles);
    serial_.tick(tcycles);
    if (cart_) {
        cart_->tick(tcycles);
    }
    if (dma_active_ || dma_start_delay_ > 0) {
        step_oam_dma(tcycles);
    }
}

u8 Bus::read(u16 addr) {
    tick(4);
    // The corruption happens even though the read itself is blocked in mode 2.
    if ((addr & 0xFF00) == 0xFE00) {
        ppu_.oam_bug_read();
    }
    if (dma_active_) {
        return dma_cpu_read(addr);
    }
    return bus_read(addr);
}

void Bus::write(u16 addr, u8 value) {
    tick(4);
    if ((addr & 0xFF00) == 0xFE00) {
        ppu_.oam_bug_write();
    }
    if (dma_active_ && dma_blocks_write(addr)) {
        return;
    }
    bus_write(addr, value);
}

void Bus::idu_pulse(u16 value) {
    if ((value & 0xFF00) == 0xFE00) {
        ppu_.oam_bug_write();
    }
}

u8 Bus::read_idu(u16 addr) {
    tick(4);
    if ((addr & 0xFF00) == 0xFE00) {
        ppu_.oam_bug_read_write();
    }
    if (dma_active_) {
        return dma_cpu_read(addr);
    }
    return bus_read(addr);
}

u8 Bus::bus_read(u16 addr) const {
    if (addr < 0x8000) {
        return cart_ ? cart_->read_rom(addr) : 0xFF;
    }
    if (addr < 0xA000) {
        return ppu_.read_vram(addr);
    }
    if (addr < 0xC000) {
        return cart_ ? cart_->read_ram(addr) : 0xFF;
    }
    if (addr < 0xE000) {
        return wram_[addr - 0xC000];
    }
    if (addr < 0xFE00) {
        return wram_[addr - 0xE000];
    }
    if (addr < 0xFEA0) {
        return ppu_.read_oam(addr);
    }
    if (addr < 0xFF00) {
        return 0x00;
    }
    if (addr < 0xFF80) {
        return io_read(addr);
    }
    if (addr < 0xFFFF) {
        return hram_[addr - 0xFF80];
    }
    return ie_;
}

void Bus::bus_write(u16 addr, u8 value) {
    if (addr < 0x8000) {
        if (cart_) {
            cart_->write_rom(addr, value);
        }
        return;
    }
    if (addr < 0xA000) {
        ppu_.write_vram(addr, value);
        return;
    }
    if (addr < 0xC000) {
        if (cart_) {
            cart_->write_ram(addr, value);
        }
        return;
    }
    if (addr < 0xE000) {
        wram_[addr - 0xC000] = value;
        return;
    }
    if (addr < 0xFE00) {
        wram_[addr - 0xE000] = value;
        return;
    }
    if (addr < 0xFEA0) {
        ppu_.write_oam(addr, value);
        return;
    }
    if (addr < 0xFF00) {
        return;
    }
    if (addr < 0xFF80) {
        io_write(addr, value);
        return;
    }
    if (addr < 0xFFFF) {
        hram_[addr - 0xFF80] = value;
        return;
    }
    ie_ = value;
}

u8 Bus::io_read(u16 addr) const {
    switch (addr) {
        case 0xFF00: return joypad_.read();
        case 0xFF01:
        case 0xFF02: return serial_.read(addr);
        case 0xFF04:
        case 0xFF05:
        case 0xFF06:
        case 0xFF07: return timer_.read(addr);
        case 0xFF0F: return if_reg();
        case 0xFF46: return dma_page_;
        default: break;
    }
    if (addr >= 0xFF10 && addr <= 0xFF3F) {
        return apu_.read(addr);
    }
    if (addr >= 0xFF40 && addr <= 0xFF4B) {
        return ppu_.read(addr);
    }
    return 0xFF;
}

void Bus::io_write(u16 addr, u8 value) {
    switch (addr) {
        case 0xFF00: joypad_.write(value); return;
        case 0xFF01:
        case 0xFF02: serial_.write(addr, value); return;
        case 0xFF04:
        case 0xFF05:
        case 0xFF06:
        case 0xFF07: timer_.write(addr, value); return;
        case 0xFF0F: write_if(value); return;
        case 0xFF46: start_oam_dma(value); return;
        default: break;
    }
    if (addr >= 0xFF10 && addr <= 0xFF3F) {
        apu_.write(addr, value);
        return;
    }
    if (addr >= 0xFF40 && addr <= 0xFF4B) {
        ppu_.write(addr, value);
    }
}

u8 Bus::peek(u16 addr) const {
    if (addr >= 0x8000 && addr < 0xA000) {
        return ppu_.peek_vram(addr);
    }
    if (addr >= 0xFE00 && addr < 0xFEA0) {
        return ppu_.peek_oam(addr);
    }
    return bus_read(addr);
}

void Bus::poke(u16 addr, u8 value) { bus_write(addr, value); }

// Source pages 0x80..0x9F read VRAM and so drive the video bus; everything else
// reads ROM, cartridge RAM or WRAM and drives the external one.
Bus::BusKind Bus::dma_bus() const {
    return (dma_page_ >= 0x80 && dma_page_ < 0xA0) ? BusKind::Video : BusKind::External;
}

u8 Bus::dma_source_read(u16 src) const {
    // Pages from 0xE0 up alias WRAM the way echo RAM does; the transfer never
    // reads OAM or the I/O page back into itself.
    if (src >= 0xE000) {
        return wram_[src & 0x1FFF];
    }
    return bus_read(src);
}

u8 Bus::dma_cpu_read(u16 addr) const {
    // The I/O page, HRAM and IE sit inside the CPU and stay readable throughout,
    // which is why a DMA wait loop can live in HRAM whatever the source is.
    if (addr >= 0xFF00) {
        return bus_read(addr);
    }
    // OAM belongs to the transfer for its whole duration.
    if (addr >= 0xFE00) {
        return 0xFF;
    }
    if (bus_kind(addr) == dma_bus()) {
        return dma_latch_;
    }
    return bus_read(addr);
}

bool Bus::dma_blocks_write(u16 addr) const {
    if (addr >= 0xFF00) {
        return false;
    }
    if (addr >= 0xFE00) {
        return true;
    }
    return bus_kind(addr) == dma_bus();
}

void Bus::start_oam_dma(u8 page) {
    // The write only arms the transfer; the bus is taken two M-cycles later. A
    // write landing on a running transfer lets that one keep the bus until the
    // new one takes over (mooneye oam_dma_restart).
    //
    // The constant is three M-cycles rather than two because an access resolves
    // after its tick: Bus::read samples dma_active_ once the M-cycle it sits in
    // has already been counted, so it would see the transfer one M-cycle sooner
    // than hardware does. Arming a cycle later cancels that out. Anything from 9
    // to 12 lands the takeover in the same M-cycle; 12 is the one that keeps the
    // one-byte-per-M-cycle pacing in phase with the CPU.
    dma_pending_page_ = page;
    dma_start_delay_ = 12;
}

void Bus::step_oam_dma(u64 tcycles) {
    for (u64 i = 0; i < tcycles; ++i) {
        if (dma_start_delay_ > 0) {
            --dma_start_delay_;
            if (dma_start_delay_ == 0) {
                dma_page_ = dma_pending_page_;
                dma_index_ = 0;
                dma_delay_ = 0;
                dma_active_ = true;
            }
        }
        if (!dma_active_) {
            continue;
        }
        if (dma_delay_ > 0) {
            --dma_delay_;
            continue;
        }
        const u16 src = static_cast<u16>((static_cast<u16>(dma_page_) << 8) | dma_index_);
        dma_latch_ = dma_source_read(src);
        ppu_.dma_write_oam(static_cast<u8>(dma_index_), dma_latch_);
        ++dma_index_;
        if (dma_index_ >= 0xA0) {
            dma_active_ = false;
            continue;
        }
        // Three idle cycles plus the transfer cycle gives one byte per M-cycle,
        // so the whole copy spans 160 M-cycles (mooneye oam_dma_timing).
        dma_delay_ = 3;
    }
}

template <typename Ar>
void Bus::visit(Ar& ar) {
    sched_.visit(ar);
    timer_.visit(ar);
    ppu_.visit(ar);
    apu_.visit(ar);
    serial_.visit(ar);
    joypad_.visit(ar);
    ar.bytes(wram_.data(), wram_.size());
    ar.bytes(hram_.data(), hram_.size());
    ar(if_);
    ar(ie_);
    ar(dma_page_);
    ar(dma_index_);
    ar(dma_delay_);
    ar(dma_active_);
    ar(dma_latch_);
    ar(dma_pending_page_);
    ar(dma_start_delay_);
}

GB_INSTANTIATE_VISIT(Bus);

}  // namespace gb
