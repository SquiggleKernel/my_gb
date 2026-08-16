#include "png.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <vector>

namespace gbtest {

namespace {

using u8 = std::uint8_t;
using u32 = std::uint32_t;

std::array<u32, 256> make_crc_table() {
    std::array<u32, 256> table{};
    for (u32 n = 0; n < 256; ++n) {
        u32 c = n;
        for (int k = 0; k < 8; ++k) {
            c = (c & 1) != 0 ? 0xEDB88320U ^ (c >> 1) : c >> 1;
        }
        table[n] = c;
    }
    return table;
}

u32 crc32(const u8* data, std::size_t len) {
    static const std::array<u32, 256> table = make_crc_table();
    u32 c = 0xFFFFFFFFU;
    for (std::size_t i = 0; i < len; ++i) {
        c = table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFU;
}

u32 adler32(const u8* data, std::size_t len) {
    u32 a = 1;
    u32 b = 0;
    for (std::size_t i = 0; i < len; ++i) {
        a = (a + data[i]) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}

void push_be32(std::vector<u8>& out, u32 v) {
    out.push_back(static_cast<u8>(v >> 24));
    out.push_back(static_cast<u8>(v >> 16));
    out.push_back(static_cast<u8>(v >> 8));
    out.push_back(static_cast<u8>(v));
}

void push_chunk(std::vector<u8>& out, const char* type, const std::vector<u8>& data) {
    push_be32(out, static_cast<u32>(data.size()));
    std::vector<u8> body;
    body.reserve(4 + data.size());
    for (int i = 0; i < 4; ++i) {
        body.push_back(static_cast<u8>(type[i]));
    }
    body.insert(body.end(), data.begin(), data.end());
    out.insert(out.end(), body.begin(), body.end());
    push_be32(out, crc32(body.data(), body.size()));
}

std::vector<u8> stored_zlib(const std::vector<u8>& raw) {
    std::vector<u8> out;
    out.push_back(0x78);
    out.push_back(0x01);
    std::size_t pos = 0;
    do {
        const std::size_t n = std::min<std::size_t>(raw.size() - pos, 65535);
        const bool last = pos + n >= raw.size();
        out.push_back(static_cast<u8>(last ? 1 : 0));
        out.push_back(static_cast<u8>(n & 0xFF));
        out.push_back(static_cast<u8>((n >> 8) & 0xFF));
        const u32 nlen = static_cast<u32>(~n);
        out.push_back(static_cast<u8>(nlen & 0xFF));
        out.push_back(static_cast<u8>((nlen >> 8) & 0xFF));
        out.insert(out.end(), raw.begin() + static_cast<std::ptrdiff_t>(pos),
                   raw.begin() + static_cast<std::ptrdiff_t>(pos + n));
        pos += n;
    } while (pos < raw.size());
    push_be32(out, adler32(raw.data(), raw.size()));
    return out;
}

}  // namespace

bool write_grey_png(const std::string& path, const u8* pixels, int width, int height) {
    if (width <= 0 || height <= 0) {
        return false;
    }

    std::vector<u8> raw;
    raw.reserve(static_cast<std::size_t>(height) * (static_cast<std::size_t>(width) + 1));
    for (int y = 0; y < height; ++y) {
        raw.push_back(0);
        const u8* row = pixels + static_cast<std::size_t>(y) * static_cast<std::size_t>(width);
        raw.insert(raw.end(), row, row + width);
    }

    std::vector<u8> out = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};

    std::vector<u8> ihdr;
    push_be32(ihdr, static_cast<u32>(width));
    push_be32(ihdr, static_cast<u32>(height));
    ihdr.push_back(8);
    ihdr.push_back(0);
    ihdr.push_back(0);
    ihdr.push_back(0);
    ihdr.push_back(0);
    push_chunk(out, "IHDR", ihdr);
    push_chunk(out, "IDAT", stored_zlib(raw));
    push_chunk(out, "IEND", {});

    std::ofstream f(path, std::ios::binary);
    if (!f) {
        return false;
    }
    f.write(reinterpret_cast<const char*>(out.data()), static_cast<std::streamsize>(out.size()));
    return static_cast<bool>(f);
}

}  // namespace gbtest
