#include "core/ppu/ppu.h"

#include <algorithm>
#include <array>

#include "core/bus.h"
#include "core/irq.h"

namespace gb {

namespace {

constexpr u16 kOamScanDots = 80;
constexpr u8 kVBlankLine = 144;
constexpr int kMaxSpritesPerLine = 10;

struct Sprite {
    u8 y;
    u8 x;
    u8 tile;
    u8 attr;
    u8 index;
};

u8 shade(u8 palette, u8 color) { return static_cast<u8>((palette >> (color * 2)) & 0x03); }

}  // namespace

void Ppu::reset() {
    vram_.fill(0);
    oam_.fill(0);
    fb_.fill(0);
    lcdc_ = 0x91;
    stat_ = 0x85;
    scy_ = 0;
    scx_ = 0;
    ly_ = 0;
    lyc_ = 0;
    bgp_ = 0xFC;
    obp0_ = 0xFF;
    obp1_ = 0xFF;
    wy_ = 0;
    wx_ = 0;
    mode_ = PpuMode::OamScan;
    dot_ = 0;
    window_line_ = 0;
    stat_line_ = false;
    frame_ready_ = false;
}

void Ppu::tick(u64 tcycles) {
    if (!lcd_on()) {
        return;
    }
    for (u64 i = 0; i < tcycles; ++i) {
        ++dot_;
        if (ly_ < kVBlankLine) {
            // Mode 3 is stretched by the fine scroll; the fetcher discards
            // SCX % 8 pixels before the first one reaches the screen.
            const u16 draw_end = static_cast<u16>(kOamScanDots + 172 + (scx_ & 7));
            if (dot_ == kOamScanDots) {
                set_mode(PpuMode::Drawing);
            } else if (dot_ == draw_end) {
                render_scanline();
                set_mode(PpuMode::HBlank);
            }
        }
        if (dot_ >= kDotsPerLine) {
            dot_ = 0;
            const u8 next = static_cast<u8>(ly_ + 1 >= kLinesPerFrame ? 0 : ly_ + 1);
            enter_line(next);
        }
    }
}

void Ppu::enter_line(u8 line) {
    ly_ = line;
    if (line == 0) {
        window_line_ = 0;
    }
    if (line < kVBlankLine) {
        set_mode(PpuMode::OamScan);
    } else if (line == kVBlankLine) {
        set_mode(PpuMode::VBlank);
        frame_ready_ = true;
        if (bus_ != nullptr) {
            bus_->request_irq(kIrqVBlank);
        }
    } else {
        update_stat_line();
    }
}

void Ppu::set_mode(PpuMode m) {
    mode_ = m;
    stat_ = static_cast<u8>((stat_ & 0xFC) | static_cast<u8>(m));
    update_stat_line();
}

void Ppu::update_stat_line() {
    const bool lyc_eq = ly_ == lyc_;
    stat_ = static_cast<u8>((stat_ & ~0x04) | (lyc_eq ? 0x04 : 0x00));

    bool line = false;
    if ((stat_ & 0x40) != 0 && lyc_eq) {
        line = true;
    }
    if ((stat_ & 0x20) != 0 && mode_ == PpuMode::OamScan) {
        line = true;
    }
    if ((stat_ & 0x10) != 0 && mode_ == PpuMode::VBlank) {
        line = true;
    }
    if ((stat_ & 0x08) != 0 && mode_ == PpuMode::HBlank) {
        line = true;
    }

    // STAT fires on the rising edge of the ORed condition, not on each source.
    if (line && !stat_line_ && bus_ != nullptr) {
        bus_->request_irq(kIrqStat);
    }
    stat_line_ = line;
}

bool Ppu::take_frame() {
    const bool ready = frame_ready_;
    frame_ready_ = false;
    return ready;
}

void Ppu::render_scanline() {
    const std::size_t row = static_cast<std::size_t>(ly_) * static_cast<std::size_t>(kScreenWidth);
    std::array<u8, kScreenWidth> bg_color{};
    bg_color.fill(0);

    const bool bg_enabled = (lcdc_ & 0x01) != 0;
    const bool win_enabled = (lcdc_ & 0x20) != 0 && bg_enabled && ly_ >= wy_ && wx_ <= 166;
    const u16 tile_data = (lcdc_ & 0x10) != 0 ? 0x8000 : 0x9000;
    const bool signed_index = (lcdc_ & 0x10) == 0;
    const u16 bg_map = (lcdc_ & 0x08) != 0 ? 0x9C00 : 0x9800;
    const u16 win_map = (lcdc_ & 0x40) != 0 ? 0x9C00 : 0x9800;

    bool window_drawn = false;
    // The tile row only changes every eighth pixel, so the map fetch and the
    // two pattern fetches are hoisted out of the per-pixel path.
    bool have_tile = false;
    bool cached_window = false;
    u8 bg_lo = 0;
    u8 bg_hi = 0;

    for (int x = 0; x < kScreenWidth; ++x) {
        u8 color = 0;
        if (bg_enabled) {
            const bool in_window = win_enabled && x + 7 >= static_cast<int>(wx_);
            const int fine_x =
                in_window ? ((x + 7 - static_cast<int>(wx_)) % 8) : ((static_cast<int>(scx_) + x) % 8);

            if (!have_tile || in_window != cached_window || fine_x == 0) {
                u16 map_base;
                u8 tx;
                u8 ty;
                if (in_window) {
                    window_drawn = true;
                    map_base = win_map;
                    tx = static_cast<u8>((((x + 7 - static_cast<int>(wx_)) / 8) & 31));
                    ty = static_cast<u8>(window_line_ / 8);
                } else {
                    map_base = bg_map;
                    tx = static_cast<u8>(((static_cast<int>(scx_) + x) / 8) & 31);
                    ty = static_cast<u8>((((static_cast<int>(scy_) + ly_) / 8) & 31));
                }

                const u16 map_addr =
                    static_cast<u16>(map_base + static_cast<u16>(ty) * 32 + static_cast<u16>(tx));
                const u8 tile = vram_[static_cast<std::size_t>(map_addr & 0x1FFF)];

                u16 tile_addr;
                if (signed_index) {
                    const int off = static_cast<int>(static_cast<i8>(tile)) * 16;
                    tile_addr = static_cast<u16>(static_cast<int>(tile_data) + off);
                } else {
                    tile_addr = static_cast<u16>(tile_data + static_cast<u16>(tile) * 16);
                }

                const int fine_y =
                    in_window ? (window_line_ % 8) : ((static_cast<int>(scy_) + ly_) % 8);
                const std::size_t lo_idx =
                    static_cast<std::size_t>((tile_addr + fine_y * 2) & 0x1FFF);
                bg_lo = vram_[lo_idx];
                bg_hi = vram_[(lo_idx + 1) & 0x1FFF];
                cached_window = in_window;
                have_tile = true;
            }

            const int bit = 7 - fine_x;
            color = static_cast<u8>((((bg_hi >> bit) & 1) << 1) | ((bg_lo >> bit) & 1));
        }
        bg_color[static_cast<std::size_t>(x)] = color;
        fb_[row + static_cast<std::size_t>(x)] = shade(bgp_, color);
    }

    if (window_drawn) {
        ++window_line_;
    }

    if ((lcdc_ & 0x02) == 0) {
        return;
    }

    const int sprite_h = (lcdc_ & 0x04) != 0 ? 16 : 8;
    std::array<Sprite, kMaxSpritesPerLine> line_sprites{};
    int count = 0;
    for (int i = 0; i < 40 && count < kMaxSpritesPerLine; ++i) {
        const std::size_t base = static_cast<std::size_t>(i) * 4;
        const int sy = static_cast<int>(oam_[base]) - 16;
        if (ly_ < sy || ly_ >= sy + sprite_h) {
            continue;
        }
        line_sprites[static_cast<std::size_t>(count)] = Sprite{oam_[base],
                                                               oam_[base + 1],
                                                               oam_[base + 2],
                                                               oam_[base + 3],
                                                               static_cast<u8>(i)};
        ++count;
    }

    // DMG priority: smaller X wins, ties broken by OAM order. Drawing back to
    // front means the winner is written last.
    std::stable_sort(line_sprites.begin(), line_sprites.begin() + count,
                     [](const Sprite& a, const Sprite& b) {
                         if (a.x != b.x) {
                             return a.x > b.x;
                         }
                         return a.index > b.index;
                     });

    for (int s = 0; s < count; ++s) {
        const Sprite& sp = line_sprites[static_cast<std::size_t>(s)];
        const int sx = static_cast<int>(sp.x) - 8;
        const int sy = static_cast<int>(sp.y) - 16;
        const bool flip_x = (sp.attr & 0x20) != 0;
        const bool flip_y = (sp.attr & 0x40) != 0;
        const bool behind_bg = (sp.attr & 0x80) != 0;
        const u8 palette = (sp.attr & 0x10) != 0 ? obp1_ : obp0_;

        int row_in = ly_ - sy;
        if (flip_y) {
            row_in = sprite_h - 1 - row_in;
        }
        u8 tile = sp.tile;
        if (sprite_h == 16) {
            tile = static_cast<u8>(tile & 0xFE);
        }
        const u16 tile_addr = static_cast<u16>(0x8000 + static_cast<u16>(tile) * 16);
        const std::size_t lo_idx = static_cast<std::size_t>((tile_addr + row_in * 2) & 0x1FFF);
        const u8 lo = vram_[lo_idx];
        const u8 hi = vram_[(lo_idx + 1) & 0x1FFF];

        for (int px = 0; px < 8; ++px) {
            const int x = sx + px;
            if (x < 0 || x >= kScreenWidth) {
                continue;
            }
            const int bit = flip_x ? px : 7 - px;
            const u8 color = static_cast<u8>((((hi >> bit) & 1) << 1) | ((lo >> bit) & 1));
            if (color == 0) {
                continue;
            }
            if (behind_bg && bg_color[static_cast<std::size_t>(x)] != 0) {
                continue;
            }
            fb_[row + static_cast<std::size_t>(x)] = shade(palette, color);
        }
    }
}

u8 Ppu::read(u16 addr) const {
    switch (addr) {
        case 0xFF40: return lcdc_;
        case 0xFF41: return static_cast<u8>(stat_ | 0x80);
        case 0xFF42: return scy_;
        case 0xFF43: return scx_;
        case 0xFF44: return ly_;
        case 0xFF45: return lyc_;
        case 0xFF47: return bgp_;
        case 0xFF48: return obp0_;
        case 0xFF49: return obp1_;
        case 0xFF4A: return wy_;
        case 0xFF4B: return wx_;
        default: return 0xFF;
    }
}

void Ppu::write(u16 addr, u8 value) {
    switch (addr) {
        case 0xFF40: {
            const bool was_on = lcd_on();
            lcdc_ = value;
            if (was_on && !lcd_on()) {
                ly_ = 0;
                dot_ = 0;
                window_line_ = 0;
                mode_ = PpuMode::HBlank;
                stat_ = static_cast<u8>(stat_ & 0xFC);
                stat_line_ = false;
                fb_.fill(0);
            } else if (!was_on && lcd_on()) {
                dot_ = 0;
                ly_ = 0;
                set_mode(PpuMode::OamScan);
            }
            break;
        }
        case 0xFF41:
            // Bits 0-2 are read-only status.
            stat_ = static_cast<u8>((value & 0x78) | (stat_ & 0x07));
            update_stat_line();
            break;
        case 0xFF42: scy_ = value; break;
        case 0xFF43: scx_ = value; break;
        case 0xFF44: break;
        case 0xFF45:
            lyc_ = value;
            update_stat_line();
            break;
        case 0xFF47: bgp_ = value; break;
        case 0xFF48: obp0_ = value; break;
        case 0xFF49: obp1_ = value; break;
        case 0xFF4A: wy_ = value; break;
        case 0xFF4B: wx_ = value; break;
        default: break;
    }
}

u8 Ppu::read_vram(u16 addr) const {
    if (lcd_on() && mode_ == PpuMode::Drawing) {
        return 0xFF;
    }
    return vram_[static_cast<std::size_t>(addr & 0x1FFF)];
}

void Ppu::write_vram(u16 addr, u8 value) {
    if (lcd_on() && mode_ == PpuMode::Drawing) {
        return;
    }
    vram_[static_cast<std::size_t>(addr & 0x1FFF)] = value;
}

u8 Ppu::read_oam(u16 addr) const {
    if (lcd_on() && (mode_ == PpuMode::Drawing || mode_ == PpuMode::OamScan)) {
        return 0xFF;
    }
    return oam_[static_cast<std::size_t>(addr - 0xFE00)];
}

void Ppu::write_oam(u16 addr, u8 value) {
    if (lcd_on() && (mode_ == PpuMode::Drawing || mode_ == PpuMode::OamScan)) {
        return;
    }
    oam_[static_cast<std::size_t>(addr - 0xFE00)] = value;
}

template <typename Ar>
void Ppu::visit(Ar& ar) {
    ar.bytes(vram_.data(), vram_.size());
    ar.bytes(oam_.data(), oam_.size());
    ar.bytes(fb_.data(), fb_.size());
    ar(lcdc_);
    ar(stat_);
    ar(scy_);
    ar(scx_);
    ar(ly_);
    ar(lyc_);
    ar(bgp_);
    ar(obp0_);
    ar(obp1_);
    ar(wy_);
    ar(wx_);
    u8 mode = static_cast<u8>(mode_);
    ar(mode);
    mode_ = static_cast<PpuMode>(mode);
    ar(dot_);
    ar(window_line_);
    ar(stat_line_);
    ar(frame_ready_);
}

GB_INSTANTIATE_VISIT(Ppu);

}  // namespace gb
