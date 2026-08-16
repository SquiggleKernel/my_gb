#include "sha256.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

namespace gbtest {

namespace {

using u8 = std::uint8_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

constexpr std::array<u32, 64> kK = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

u32 ror(u32 v, int n) { return (v >> n) | (v << (32 - n)); }

void compress(std::array<u32, 8>& h, const u8* block) {
    std::array<u32, 64> w{};
    for (int i = 0; i < 16; ++i) {
        w[static_cast<std::size_t>(i)] = static_cast<u32>(block[i * 4]) << 24 |
                                         static_cast<u32>(block[i * 4 + 1]) << 16 |
                                         static_cast<u32>(block[i * 4 + 2]) << 8 |
                                         static_cast<u32>(block[i * 4 + 3]);
    }
    for (std::size_t i = 16; i < 64; ++i) {
        const u32 s0 = ror(w[i - 15], 7) ^ ror(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const u32 s1 = ror(w[i - 2], 17) ^ ror(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    u32 a = h[0], b = h[1], c = h[2], d = h[3];
    u32 e = h[4], f = h[5], g = h[6], hh = h[7];

    for (std::size_t i = 0; i < 64; ++i) {
        const u32 s1 = ror(e, 6) ^ ror(e, 11) ^ ror(e, 25);
        const u32 ch = (e & f) ^ (~e & g);
        const u32 t1 = hh + s1 + ch + kK[i] + w[i];
        const u32 s0 = ror(a, 2) ^ ror(a, 13) ^ ror(a, 22);
        const u32 maj = (a & b) ^ (a & c) ^ (b & c);
        const u32 t2 = s0 + maj;
        hh = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
    h[5] += f;
    h[6] += g;
    h[7] += hh;
}

}  // namespace

std::string sha256_hex(const void* data, std::size_t size) {
    std::array<u32, 8> h = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                            0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

    std::vector<u8> buf(static_cast<const u8*>(data), static_cast<const u8*>(data) + size);
    const u64 bits = static_cast<u64>(size) * 8;
    buf.push_back(0x80);
    while (buf.size() % 64 != 56) {
        buf.push_back(0);
    }
    for (int i = 7; i >= 0; --i) {
        buf.push_back(static_cast<u8>((bits >> (i * 8)) & 0xFF));
    }
    for (std::size_t off = 0; off < buf.size(); off += 64) {
        compress(h, buf.data() + off);
    }

    static const char* kHex = "0123456789abcdef";
    std::string out;
    out.reserve(64);
    for (u32 word : h) {
        for (int i = 7; i >= 0; --i) {
            out.push_back(kHex[(word >> (i * 4)) & 0xF]);
        }
    }
    return out;
}

}  // namespace gbtest
