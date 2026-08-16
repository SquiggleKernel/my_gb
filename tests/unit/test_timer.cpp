#include <catch2/catch_test_macros.hpp>

#include "core/bus.h"
#include "core/irq.h"

namespace {

// The timer is edge-driven off the internal counter, so it can only be
// exercised meaningfully through a Bus that ticks every component together.
struct Fixture {
    gb::Bus bus;

    Fixture() {
        bus.write_if(0);
        bus.timer().write(0xFF04, 0);
    }

    void tick(gb::u64 n) { bus.tick(n); }
    gb::u8 div() { return bus.timer().read(0xFF04); }
    gb::u8 tima() { return bus.timer().read(0xFF05); }
    void set(gb::u16 addr, gb::u8 v) { bus.timer().write(addr, v); }
    bool timer_irq() const { return (bus.if_reg() & gb::kIrqTimer) != 0; }
};

}  // namespace

TEST_CASE("DIV is the top byte of a counter running at the master clock", "[timer]") {
    Fixture f;
    REQUIRE(f.div() == 0);
    f.tick(255);
    REQUIRE(f.div() == 0);
    f.tick(1);
    REQUIRE(f.div() == 1);
    f.tick(256 * 3);
    REQUIRE(f.div() == 4);
}

TEST_CASE("writing DIV resets the whole counter", "[timer]") {
    Fixture f;
    f.tick(1000);
    REQUIRE(f.div() != 0);
    f.set(0xFF04, 0x99);
    REQUIRE(f.div() == 0);
}

TEST_CASE("TIMA counts at the rate TAC selects", "[timer]") {
    Fixture f;
    // TAC 0x05: enabled, bit 3 of the counter, so one tick every 16 T-cycles.
    f.set(0xFF07, 0x05);
    f.set(0xFF05, 0);
    f.tick(16);
    REQUIRE(f.tima() == 1);
    f.tick(16 * 4);
    REQUIRE(f.tima() == 5);
}

TEST_CASE("a disabled timer does not count", "[timer]") {
    Fixture f;
    f.set(0xFF07, 0x01);
    f.set(0xFF05, 0);
    f.tick(4096);
    REQUIRE(f.tima() == 0);
}

TEST_CASE("TIMA overflow reloads TMA four cycles late and fires the interrupt", "[timer]") {
    Fixture f;
    f.set(0xFF07, 0x05);
    f.set(0xFF06, 0x42);
    f.set(0xFF05, 0xFF);

    f.tick(16);
    // The reload has not happened yet: TIMA reads zero and no interrupt.
    REQUIRE(f.tima() == 0x00);
    REQUIRE_FALSE(f.timer_irq());

    f.tick(4);
    REQUIRE(f.tima() == 0x42);
    REQUIRE(f.timer_irq());
}

TEST_CASE("writing TIMA inside the reload window cancels the reload", "[timer]") {
    Fixture f;
    f.set(0xFF07, 0x05);
    f.set(0xFF06, 0x42);
    f.set(0xFF05, 0xFF);

    f.tick(16);
    REQUIRE(f.tima() == 0x00);
    f.set(0xFF05, 0x7E);

    f.tick(4);
    REQUIRE(f.tima() == 0x7E);
    REQUIRE_FALSE(f.timer_irq());
}

TEST_CASE("TMA written during the reload cycle feeds straight through", "[timer]") {
    Fixture f;
    f.set(0xFF07, 0x05);
    f.set(0xFF06, 0x42);
    f.set(0xFF05, 0xFF);

    f.tick(16);
    f.tick(3);
    f.set(0xFF06, 0x11);
    f.tick(1);
    REQUIRE(f.tima() == 0x11);
}

TEST_CASE("resetting DIV can itself clock TIMA", "[timer]") {
    Fixture f;
    // TAC 0x04 watches counter bit 9; get the counter above that bit so the
    // reset produces a falling edge.
    f.set(0xFF07, 0x04);
    f.set(0xFF05, 0);
    f.tick(512);
    const gb::u8 before = f.tima();
    f.set(0xFF04, 0);
    REQUIRE(f.tima() == static_cast<gb::u8>(before + 1));
}

TEST_CASE("unused TAC bits read as one", "[timer]") {
    Fixture f;
    f.set(0xFF07, 0x05);
    REQUIRE(f.bus.timer().read(0xFF07) == 0xFD);
}
