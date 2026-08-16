#pragma once

#include <cstddef>
#include <cstdint>

namespace gb {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

inline constexpr int kScreenWidth = 160;
inline constexpr int kScreenHeight = 144;
inline constexpr std::size_t kFramebufferPixels =
    static_cast<std::size_t>(kScreenWidth) * static_cast<std::size_t>(kScreenHeight);

// A DMG frame is 154 scanlines of 456 dots with the LCD enabled.
inline constexpr u64 kTCyclesPerFrame = 70224;
inline constexpr u64 kTCyclesPerSecond = 4194304;

}  // namespace gb
