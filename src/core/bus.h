#pragma once

#include <array>
#include <memory>

#include "core/apu/apu.h"
#include "core/cart/cartridge.h"
#include "core/irq.h"
#include "core/joypad.h"
#include "core/ppu/ppu.h"
#include "core/scheduler.h"
#include "core/serial.h"
#include "core/state/serialize.h"
#include "core/timer.h"
#include "core/types.h"

namespace gb {

// read()/write() tick four T-cycles before the access resolves. Every other
// internal CPU cycle goes through tick(). Nothing in the core may touch memory
// without advancing time first -- that is what makes mem_timing and oam_bug
// reachable.
class Bus {
  public:
    Bus();

    // Each component caches this Bus, so copying or moving one would leave the
    // components attached to the wrong machine.
    Bus(const Bus&) = delete;
    Bus& operator=(const Bus&) = delete;
    Bus(Bus&&) = delete;
    Bus& operator=(Bus&&) = delete;

    void reset();

    void load_cartridge(std::unique_ptr<Cartridge> cart);
    Cartridge* cartridge() const { return cart_.get(); }

    u8 read(u16 addr);
    void write(u16 addr, u8 value);
    void tick(u64 tcycles);

    // A 16-bit increment or decrement drives the register onto the address bus
    // even with no read or write asserted, so an OAM address corrupts OAM.
    void idu_pulse(u16 value);

    // A read whose M-cycle also runs the increment unit over the same address.
    // That combination corrupts OAM differently from a plain read.
    u8 read_idu(u16 addr);

    // Side-effect free; for the debugger, tracer and tests.
    u8 peek(u16 addr) const;
    void poke(u16 addr, u8 value);

    void request_irq(u8 mask) { if_ = static_cast<u8>(if_ | mask); }
    void clear_irq(u8 mask) { if_ = static_cast<u8>(if_ & ~mask); }
    u8 if_reg() const { return static_cast<u8>(if_ | 0xE0); }
    u8 ie_reg() const { return ie_; }
    void write_if(u8 v) { if_ = static_cast<u8>(v & 0x1F); }
    void write_ie(u8 v) { ie_ = v; }
    u8 pending_irqs() const { return static_cast<u8>(if_ & ie_ & 0x1F); }

    u64 cycles() const { return sched_.now(); }

    Scheduler& scheduler() { return sched_; }
    Timer& timer() { return timer_; }
    Ppu& ppu() { return ppu_; }
    const Ppu& ppu() const { return ppu_; }
    Apu& apu() { return apu_; }
    Serial& serial() { return serial_; }
    Joypad& joypad() { return joypad_; }

    bool oam_dma_active() const { return dma_active_; }

    template <typename Ar>
    void visit(Ar& ar);

  private:
    // The DMG splits memory over two buses that can be driven independently.
    // OAM DMA takes whichever one its source page sits on and leaves the other
    // to the CPU, so which bus an address belongs to decides whether a CPU
    // access during a transfer conflicts with it.
    enum class BusKind : u8 { External, Video, Internal };

    static constexpr BusKind bus_kind(u16 addr) {
        if (addr < 0x8000) {
            return BusKind::External;  // ROM
        }
        if (addr < 0xA000) {
            return BusKind::Video;  // VRAM
        }
        if (addr < 0xFE00) {
            return BusKind::External;  // cartridge RAM, WRAM, echo
        }
        return BusKind::Internal;  // OAM, I/O, HRAM, IE
    }

    u8 bus_read(u16 addr) const;
    void bus_write(u16 addr, u8 value);
    u8 io_read(u16 addr) const;
    void io_write(u16 addr, u8 value);
    void start_oam_dma(u8 page);
    void step_oam_dma(u64 tcycles);

    BusKind dma_bus() const;
    u8 dma_source_read(u16 src) const;
    u8 dma_cpu_read(u16 addr) const;
    bool dma_blocks_write(u16 addr) const;

    Scheduler sched_;
    Timer timer_;
    Ppu ppu_;
    Apu apu_;
    Serial serial_;
    Joypad joypad_;
    std::unique_ptr<Cartridge> cart_;

    std::array<u8, 0x2000> wram_{};
    std::array<u8, 0x7F> hram_{};

    u8 if_ = 0;
    u8 ie_ = 0;

    u8 dma_page_ = 0xFF;
    u16 dma_index_ = 0;
    u16 dma_delay_ = 0;
    bool dma_active_ = false;
    // What the transfer last put on the bus. A CPU access that lands on the
    // busy bus reads this back instead of the byte it addressed.
    u8 dma_latch_ = 0xFF;
    // A write to FF46 does not take the bus straight away. Until the pending
    // start expires the previous transfer, if any, keeps running and keeps
    // owning the bus, which is what makes a restart observable.
    u8 dma_pending_page_ = 0xFF;
    u16 dma_start_delay_ = 0;
};

}  // namespace gb
