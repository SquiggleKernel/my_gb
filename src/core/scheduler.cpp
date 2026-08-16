#include "core/scheduler.h"

namespace gb {

void Scheduler::reset() {
    now_ = 0;
    deadline_.fill(kNever);
}

void Scheduler::schedule(EventId id, u64 delta) {
    deadline_[static_cast<std::size_t>(id)] = now_ + delta;
}

void Scheduler::cancel(EventId id) { deadline_[static_cast<std::size_t>(id)] = kNever; }

bool Scheduler::pending(EventId id) const {
    return deadline_[static_cast<std::size_t>(id)] != kNever;
}

u64 Scheduler::due_at(EventId id) const { return deadline_[static_cast<std::size_t>(id)]; }

bool Scheduler::pop_due(EventId& out) {
    std::size_t best = kEventCount;
    u64 best_time = kNever;
    for (std::size_t i = 0; i < kEventCount; ++i) {
        if (deadline_[i] <= now_ && deadline_[i] < best_time) {
            best = i;
            best_time = deadline_[i];
        }
    }
    if (best == kEventCount) {
        return false;
    }
    deadline_[best] = kNever;
    out = static_cast<EventId>(best);
    return true;
}

u64 Scheduler::next_deadline() const {
    u64 best = kNever;
    for (u64 d : deadline_) {
        if (d < best) {
            best = d;
        }
    }
    return best;
}

template <typename Ar>
void Scheduler::visit(Ar& ar) {
    ar(now_);
    ar(deadline_);
}

GB_INSTANTIATE_VISIT(Scheduler);

}  // namespace gb
