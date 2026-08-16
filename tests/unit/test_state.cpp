#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <string>
#include <vector>

#include "core/gb.h"
#include "core/state/rewind.h"
#include "core/types.h"

namespace {

// A ROM-only cart whose entry point walks WRAM writing an accumulator into it,
// so every frame moves registers, WRAM and the PPU somewhere new.
std::vector<gb::u8> build_rom() {
    std::vector<gb::u8> rom(32768, 0x00);
    rom[0x147] = 0x00;
    rom[0x148] = 0x00;
    rom[0x149] = 0x00;

    const gb::u8 program[] = {
        0x21, 0x00, 0xC0,  // 0100: LD HL,$C000
        0x3E, 0x01,        // 0103: LD A,$01
        0x87,              // 0105: ADD A,A
        0x3C,              // 0106: INC A
        0x22,              // 0107: LD (HL+),A
        0x7C,              // 0108: LD A,H
        0xFE, 0xE0,        // 0109: CP $E0
        0x20, 0xF8,        // 010B: JR NZ,$0105
        0x18, 0xF1,        // 010D: JR $0100
    };
    std::copy(std::begin(program), std::end(program), rom.begin() + 0x100);
    return rom;
}

void boot(gb::Gb& gb) {
    std::string err;
    REQUIRE(gb.load_rom(build_rom(), &err));
}

void run_frames(gb::Gb& gb, int n) {
    for (int i = 0; i < n; ++i) {
        gb.run_frame();
    }
}

gb::u64 hash_state(gb::Gb& gb) {
    const std::vector<gb::u8> blob = gb.save_state();
    gb::u64 h = 0xCBF29CE484222325ULL;
    for (gb::u8 b : blob) {
        h ^= b;
        h *= 0x100000001B3ULL;
    }
    return h;
}

}  // namespace

TEST_CASE("a loaded state replays the same timeline", "[state]") {
    gb::Gb a;
    boot(a);
    run_frames(a, 40);
    const std::vector<gb::u8> blob = a.save_state();

    run_frames(a, 25);
    const gb::u64 straight_through = hash_state(a);

    REQUIRE(a.load_state(blob));
    run_frames(a, 25);
    REQUIRE(hash_state(a) == straight_through);
}

TEST_CASE("a state round trips to a byte-identical blob", "[state]") {
    gb::Gb gb;
    boot(gb);
    run_frames(gb, 17);

    const std::vector<gb::u8> first = gb.save_state();
    REQUIRE(gb.load_state(first));
    const std::vector<gb::u8> second = gb.save_state();
    REQUIRE(first == second);
}

TEST_CASE("a truncated state is rejected and changes nothing", "[state]") {
    gb::Gb gb;
    boot(gb);
    run_frames(gb, 12);
    const std::vector<gb::u8> blob = gb.save_state();

    run_frames(gb, 9);
    const std::vector<gb::u8> before = gb.save_state();

    const std::vector<gb::u8> truncated(blob.begin(), blob.begin() + 64);
    REQUIRE_FALSE(gb.load_state(truncated));
    REQUIRE(gb.save_state() == before);

    // One byte short still has to be caught; the payload has a fixed length.
    std::vector<gb::u8> short_by_one = blob;
    short_by_one.pop_back();
    REQUIRE_FALSE(gb.load_state(short_by_one));
    REQUIRE(gb.save_state() == before);
}

TEST_CASE("a corrupt magic or version is rejected", "[state]") {
    gb::Gb gb;
    boot(gb);
    run_frames(gb, 5);
    const std::vector<gb::u8> before = gb.save_state();

    std::vector<gb::u8> bad_magic = before;
    bad_magic[0] = static_cast<gb::u8>(bad_magic[0] ^ 0xFF);
    REQUIRE_FALSE(gb.load_state(bad_magic));

    std::vector<gb::u8> bad_version = before;
    bad_version[4] = static_cast<gb::u8>(bad_version[4] + 1);
    REQUIRE_FALSE(gb.load_state(bad_version));

    REQUIRE_FALSE(gb.load_state(std::vector<gb::u8>{}));
    REQUIRE(gb.save_state() == before);
}

TEST_CASE("a state does not cross between machines with different carts", "[state]") {
    gb::Gb with_cart;
    boot(with_cart);
    run_frames(with_cart, 3);
    const std::vector<gb::u8> cart_state = with_cart.save_state();

    gb::Gb bare;
    run_frames(bare, 3);
    const std::vector<gb::u8> bare_state = bare.save_state();

    REQUIRE_FALSE(bare.load_state(cart_state));
    REQUIRE(bare.save_state() == bare_state);

    const std::vector<gb::u8> cart_before = with_cart.save_state();
    REQUIRE_FALSE(with_cart.load_state(bare_state));
    REQUIRE(with_cart.save_state() == cart_before);
}

TEST_CASE("rewind costs nothing until it is enabled", "[rewind]") {
    gb::Gb gb;
    boot(gb);
    run_frames(gb, 90);
    REQUIRE(gb.rewind().size() == 0);
    REQUIRE_FALSE(gb.rewind_step_back());
}

TEST_CASE("stepping back and replaying reproduces the same frames", "[rewind]") {
    gb::Gb gb;
    boot(gb);
    gb.set_rewind_enabled(true);

    constexpr int kFrames = 200;
    std::vector<gb::u64> marks;
    marks.reserve(static_cast<std::size_t>(kFrames));
    for (int i = 0; i < kFrames; ++i) {
        gb.run_frame();
        marks.push_back(hash_state(gb));
    }

    const std::size_t interval = gb.rewind().interval();
    REQUIRE(gb.rewind().size() == (static_cast<std::size_t>(kFrames) + interval - 1) / interval);
    REQUIRE(gb.rewind().bytes() > 0);

    for (int back = 0; back < 3; ++back) {
        REQUIRE(gb.rewind_step_back());
    }

    // The restored machine must be bit-for-bit one of the frames we passed
    // through on the way out.
    const gb::u64 landed = hash_state(gb);
    const auto it = std::find(marks.begin(), marks.end(), landed);
    REQUIRE(it != marks.end());

    std::size_t frame = static_cast<std::size_t>(it - marks.begin());
    // Three steps back has to have moved us well clear of where we stopped.
    REQUIRE(frame + 2 * interval < static_cast<std::size_t>(kFrames));

    for (int i = 0; i < 40; ++i) {
        gb.run_frame();
        ++frame;
        REQUIRE(hash_state(gb) == marks[frame]);
    }
}

TEST_CASE("rewind runs out and reports it", "[rewind]") {
    gb::Gb gb;
    boot(gb);
    gb.set_rewind_enabled(true);
    run_frames(gb, 100);

    const std::size_t buffered = gb.rewind().size();
    REQUIRE(buffered > 0);
    for (std::size_t i = 0; i < buffered; ++i) {
        REQUIRE(gb.rewind_step_back());
    }
    REQUIRE(gb.rewind().size() == 0);
    REQUIRE_FALSE(gb.rewind_step_back());
}

TEST_CASE("the ring drops the oldest snapshot once it is full", "[rewind]") {
    gb::Gb gb;
    boot(gb);
    gb.set_rewind_enabled(true);
    gb.rewind().set_capacity(4);
    gb.rewind().set_interval(2);

    run_frames(gb, 30);
    REQUIRE(gb.rewind().size() == 4);

    const std::size_t held = gb.rewind().bytes();
    run_frames(gb, 60);
    REQUIRE(gb.rewind().size() == 4);
    // Reused slots, so a much longer run must not grow what we hold.
    REQUIRE(gb.rewind().bytes() == held);

    gb.rewind().clear();
    REQUIRE(gb.rewind().size() == 0);
    REQUIRE_FALSE(gb.rewind_step_back());
}
