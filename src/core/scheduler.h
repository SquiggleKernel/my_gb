#pragma once

#include <array>

#include "core/state/serialize.h"
#include "core/types.h"

namespace gb {

enum class EventId : u8 {
    TimerOverflow,
    ApuFrameSequencer,
    PpuMode,
    OamDma,
    Serial,
    Count,
};

inline constexpr std::size_t kEventCount = static_cast<std::size_t>(EventId::Count);

// At most one pending event per id, which holds for every DMG subsystem. That
// keeps ordering deterministic (deadline, then id) without a sequence counter
// to serialise, and keeps the whole thing trivially snapshottable for rewind.
class Scheduler {
  public:
    Scheduler() { reset(); }

    void reset();

    u64 now() const { return now_; }
    void advance(u64 tcycles) { now_ += tcycles; }

    void schedule(EventId id, u64 delta);
    void cancel(EventId id);
    bool pending(EventId id) const;
    u64 due_at(EventId id) const;

    // Pops the earliest event that is due, or returns false. Call in a loop.
    bool pop_due(EventId& out);

    u64 next_deadline() const;

    template <typename Ar>
    void visit(Ar& ar);

  private:
    static constexpr u64 kNever = ~u64{0};

    u64 now_ = 0;
    std::array<u64, kEventCount> deadline_{};
};

}  // namespace gb
