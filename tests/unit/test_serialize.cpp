#include <catch2/catch_test_macros.hpp>

#include <array>
#include <vector>

#include "core/state/serialize.h"

using gb::ReadArchive;
using gb::WriteArchive;

TEST_CASE("scalars survive a round trip", "[serialize]") {
    std::vector<gb::u8> blob;
    WriteArchive w(blob);

    bool flag = true;
    gb::u8 a = 0xAB;
    gb::i8 b = -42;
    gb::u16 c = 0xBEEF;
    gb::i16 d = -300;
    gb::u32 e = 0xDEADBEEF;
    gb::i32 f = -70000;
    gb::u64 g = 0x0123456789ABCDEF;
    gb::i64 h = -5000000000LL;
    w(flag);
    w(a);
    w(b);
    w(c);
    w(d);
    w(e);
    w(f);
    w(g);
    w(h);

    bool flag2 = false;
    gb::u8 a2 = 0;
    gb::i8 b2 = 0;
    gb::u16 c2 = 0;
    gb::i16 d2 = 0;
    gb::u32 e2 = 0;
    gb::i32 f2 = 0;
    gb::u64 g2 = 0;
    gb::i64 h2 = 0;
    ReadArchive r(blob.data(), blob.size());
    r(flag2);
    r(a2);
    r(b2);
    r(c2);
    r(d2);
    r(e2);
    r(f2);
    r(g2);
    r(h2);

    REQUIRE(r.ok());
    REQUIRE(flag2 == flag);
    REQUIRE(a2 == a);
    REQUIRE(b2 == b);
    REQUIRE(c2 == c);
    REQUIRE(d2 == d);
    REQUIRE(e2 == e);
    REQUIRE(f2 == f);
    REQUIRE(g2 == g);
    REQUIRE(h2 == h);
}

TEST_CASE("encoding is little endian regardless of host", "[serialize]") {
    std::vector<gb::u8> blob;
    WriteArchive w(blob);
    gb::u32 value = 0x11223344;
    w(value);
    REQUIRE(blob == std::vector<gb::u8>{0x44, 0x33, 0x22, 0x11});
}

TEST_CASE("arrays round trip element by element", "[serialize]") {
    std::vector<gb::u8> blob;
    WriteArchive w(blob);
    std::array<gb::u16, 4> src = {1, 0x1234, 0xFFFF, 0};
    w(src);
    REQUIRE(blob.size() == 8);

    std::array<gb::u16, 4> dst{};
    ReadArchive r(blob.data(), blob.size());
    r(dst);
    REQUIRE(r.ok());
    REQUIRE(dst == src);
}

TEST_CASE("blobs round trip", "[serialize]") {
    const std::array<gb::u8, 5> src = {9, 8, 7, 6, 5};
    std::vector<gb::u8> blob;
    WriteArchive w(blob);
    w.bytes(src.data(), src.size());
    REQUIRE(blob.size() == src.size());

    std::array<gb::u8, 5> dst{};
    ReadArchive r(blob.data(), blob.size());
    r.bytes(dst.data(), dst.size());
    REQUIRE(r.ok());
    REQUIRE(dst == src);
}

TEST_CASE("truncated input is reported rather than read past the end", "[serialize]") {
    const std::array<gb::u8, 2> blob = {0x01, 0x02};

    ReadArchive r(blob.data(), blob.size());
    gb::u32 value = 0xFFFFFFFF;
    r(value);
    REQUIRE_FALSE(r.ok());

    ReadArchive r2(blob.data(), blob.size());
    std::array<gb::u8, 8> dst{};
    dst.fill(0xEE);
    r2.bytes(dst.data(), dst.size());
    REQUIRE_FALSE(r2.ok());
    REQUIRE(dst[0] == 0);
}
