#pragma once

#include <array>

#include "core/state/serialize.h"
#include "core/types.h"

namespace gb {

class Bus;

enum class PpuMode : u8 { HBlank = 0, VBlank = 1, OamScan = 2, Drawing = 3 };

inline constexpr int kDotsPerLine = 456;
inline constexpr int kLinesPerFrame = 154;

// The framebuffer holds raw 2-bit colour indices, not RGB. Frontends own the
// palette, which keeps the core free of any display concern and makes frame
// hashes stable across platforms.
class Ppu {
  public:
    void attach(Bus* bus) { bus_ = bus; }
    void reset();

    void tick(u64 tcycles);

    u8 read(u16 addr) const;
    void write(u16 addr, u8 value);

    u8 read_vram(u16 addr) const;
    void write_vram(u16 addr, u8 value);
    u8 read_oam(u16 addr) const;
    void write_oam(u16 addr, u8 value);

    // OAM DMA bypasses the mode-based access block.
    void dma_write_oam(u8 index, u8 value) { oam_[index] = value; }

    // On DMG, touching OAM during mode 2 corrupts it, and so does merely
    // putting an OAM address on the bus via the 16-bit increment unit. The
    // access address and the written value do not matter, only the kind of
    // access and which row the scan has reached.
    void oam_bug_read();
    void oam_bug_write();
    void oam_bug_read_write();

    u8 peek_vram(u16 addr) const { return vram_[addr & 0x1FFF]; }
    u8 peek_oam(u16 addr) const { return oam_[addr & 0xFF]; }

    PpuMode mode() const { return mode_; }
    u8 ly() const { return ly_; }
    // Counts only the lines the window actually drew, which is what the window
    // fetcher indexes with rather than LY - WY.
    u8 window_line() const { return window_line_; }
    bool lcd_on() const { return (lcdc_ & 0x80) != 0; }

    const std::array<u8, kFramebufferPixels>& framebuffer() const { return fb_; }
    bool take_frame();

    template <typename Ar>
    void visit(Ar& ar);

  private:
    bool oam_bug_window() const;
    int oam_row() const;
    u16 oam_word(int row, int word) const;
    void set_oam_word(int row, int word, u16 value);
    void corrupt_row(int row, u16 first);

    void set_mode(PpuMode m);
    void update_stat_line();
    void enter_line(u8 line);
    void render_scanline();
    // How long mode 3 runs on the current line. Fixed at the point drawing
    // starts, because everything it depends on is latched by then.
    u16 mode3_dots() const;

    Bus* bus_ = nullptr;

    std::array<u8, 0x2000> vram_{};
    std::array<u8, 0xA0> oam_{};
    std::array<u8, kFramebufferPixels> fb_{};

    u8 lcdc_ = 0x91;
    u8 stat_ = 0x85;
    u8 scy_ = 0;
    u8 scx_ = 0;
    u8 ly_ = 0;
    u8 lyc_ = 0;
    u8 bgp_ = 0xFC;
    u8 obp0_ = 0;
    u8 obp1_ = 0;
    u8 wy_ = 0;
    u8 wx_ = 0;

    PpuMode mode_ = PpuMode::OamScan;
    u16 dot_ = 0;
    u16 mode3_dots_ = 172;
    u8 window_line_ = 0;
    bool stat_line_ = false;
    bool frame_ready_ = false;
};

}  // namespace gb
