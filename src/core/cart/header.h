#pragma once

#include <span>
#include <string>

#include "core/types.h"

namespace gb {

enum class Mapper : u8 { None, Mbc1, Mbc2, Mbc3, Mbc5, Unsupported };

struct CartHeader {
    std::string title;
    Mapper mapper = Mapper::None;
    u8 cart_type = 0;
    u8 rom_size_code = 0;
    u8 ram_size_code = 0;
    u32 rom_bytes = 0;
    u32 ram_bytes = 0;
    bool has_battery = false;
    bool has_rtc = false;
    bool has_rumble = false;
    bool header_checksum_ok = false;
};

// Returns false and fills `err` when the ROM is too small or the mapper is not
// one we implement. Never reads outside `rom`.
bool parse_header(std::span<const u8> rom, CartHeader& out, std::string* err);

const char* mapper_name(Mapper m);

}  // namespace gb
