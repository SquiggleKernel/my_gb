#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "core/scheduler.h"
#include "core/state/serialize.h"

using gb::EventId;
using gb::Scheduler;

TEST_CASE("nothing is due on a fresh scheduler", "[scheduler]") {
    Scheduler s;
    EventId id{};
    REQUIRE_FALSE(s.pop_due(id));
    REQUIRE(s.now() == 0);
    REQUIRE_FALSE(s.pending(EventId::TimerOverflow));
    REQUIRE(s.next_deadline() == ~gb::u64{0});
}

TEST_CASE("an event only fires once its deadline has passed", "[scheduler]") {
    Scheduler s;
    s.schedule(EventId::TimerOverflow, 16);
    REQUIRE(s.pending(EventId::TimerOverflow));
    REQUIRE(s.due_at(EventId::TimerOverflow) == 16);

    EventId id{};
    s.advance(15);
    REQUIRE_FALSE(s.pop_due(id));

    s.advance(1);
    REQUIRE(s.pop_due(id));
    REQUIRE(id == EventId::TimerOverflow);
    REQUIRE_FALSE(s.pending(EventId::TimerOverflow));
    REQUIRE_FALSE(s.pop_due(id));
}

TEST_CASE("due events come out in deadline order", "[scheduler]") {
    Scheduler s;
    s.schedule(EventId::Serial, 30);
    s.schedule(EventId::TimerOverflow, 10);
    s.schedule(EventId::PpuMode, 20);
    s.advance(100);

    std::vector<EventId> order;
    EventId id{};
    while (s.pop_due(id)) {
        order.push_back(id);
    }
    REQUIRE(order == std::vector<EventId>{EventId::TimerOverflow, EventId::PpuMode,
                                          EventId::Serial});
}

TEST_CASE("ties break by event id so ordering stays deterministic", "[scheduler]") {
    Scheduler s;
    s.schedule(EventId::OamDma, 8);
    s.schedule(EventId::ApuFrameSequencer, 8);
    s.schedule(EventId::PpuMode, 8);
    s.advance(8);

    std::vector<EventId> order;
    EventId id{};
    while (s.pop_due(id)) {
        order.push_back(id);
    }
    REQUIRE(order == std::vector<EventId>{EventId::ApuFrameSequencer, EventId::PpuMode,
                                          EventId::OamDma});
}

TEST_CASE("cancel removes a pending event", "[scheduler]") {
    Scheduler s;
    s.schedule(EventId::ApuFrameSequencer, 4);
    s.cancel(EventId::ApuFrameSequencer);
    s.advance(100);

    EventId id{};
    REQUIRE_FALSE(s.pop_due(id));
    REQUIRE_FALSE(s.pending(EventId::ApuFrameSequencer));
}

TEST_CASE("next_deadline reports the earliest pending event", "[scheduler]") {
    Scheduler s;
    s.schedule(EventId::Serial, 90);
    s.schedule(EventId::PpuMode, 12);
    REQUIRE(s.next_deadline() == 12);
    s.cancel(EventId::PpuMode);
    REQUIRE(s.next_deadline() == 90);
}

TEST_CASE("a restored scheduler behaves identically", "[scheduler]") {
    Scheduler s;
    s.advance(500);
    s.schedule(EventId::TimerOverflow, 40);
    s.schedule(EventId::Serial, 10);

    std::vector<gb::u8> blob;
    gb::WriteArchive w(blob);
    s.visit(w);

    Scheduler restored;
    gb::ReadArchive r(blob.data(), blob.size());
    restored.visit(r);
    REQUIRE(r.ok());

    REQUIRE(restored.now() == s.now());
    REQUIRE(restored.due_at(EventId::TimerOverflow) == s.due_at(EventId::TimerOverflow));
    REQUIRE(restored.next_deadline() == s.next_deadline());

    restored.advance(10);
    EventId id{};
    REQUIRE(restored.pop_due(id));
    REQUIRE(id == EventId::Serial);
}
