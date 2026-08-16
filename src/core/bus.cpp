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
    if (dma_active_) {
        step_oam_dma(tcycles);
    }
}

u8 Bus::read(u16 addr) {
    tick(4);
    // While OAM DMA runs the CPU only has a clean view of HRAM.
    if (dma_active_ && (addr < 0xFF80 || addr == 0xFFFF)) {
        return 0xFF;
    }
    return bus_read(addr);
}

void Bus::write(u16 addr, u8 value) {
    tick(4);
    if (dma_active_ && (addr < 0xFF80 || addr == 0xFFFF)) {
        // The DMA register itself stays writable, restarting the transfer.
        if (addr != 0xFF46) {
            return;
        }
    }
    bus_write(addr, value);
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

void Bus::start_oam_dma(u8 page) {
    dma_page_ = page;
    dma_index_ = 0;
    // One M-cycle of setup before the first byte moves.
    dma_delay_ = 8;
    dma_active_ = true;
}

void Bus::step_oam_dma(u64 tcycles) {
    for (u64 i = 0; i < tcycles; ++i) {
        if (dma_delay_ > 0) {
            --dma_delay_;
            continue;
        }
        const u16 src = static_cast<u16>((static_cast<u16>(dma_page_) << 8) | dma_index_);
        ppu_.dma_write_oam(static_cast<u8>(dma_index_), bus_read(src));
        ++dma_index_;
        if (dma_index_ >= 0xA0) {
            dma_active_ = false;
            return;
        }
        dma_delay_ = 4;
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
}

GB_INSTANTIATE_VISIT(Bus);

}  // namespace gb
