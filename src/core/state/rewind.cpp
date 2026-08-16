#include "core/state/rewind.h"

#include <algorithm>

#include "core/gb.h"

namespace gb {

Rewind::Rewind(std::size_t interval_frames, std::size_t capacity)
    : slots_(capacity), interval_(std::max<std::size_t>(interval_frames, 1)) {}

void Rewind::set_interval(std::size_t frames) {
    interval_ = std::max<std::size_t>(frames, 1);
    clear();
}

void Rewind::set_capacity(std::size_t snapshots) {
    slots_.assign(snapshots, std::vector<u8>{});
    clear();
}

void Rewind::capture(Gb& gb) {
    if (slots_.empty()) {
        return;
    }
    const bool due = counter_ == 0;
    counter_ = (counter_ + 1) % interval_;
    if (!due) {
        return;
    }

    gb.write_state(slots_[head_]);
    head_ = (head_ + 1) % slots_.size();
    if (size_ < slots_.size()) {
        ++size_;
    }
}

bool Rewind::step_back(Gb& gb) {
    if (size_ == 0) {
        return false;
    }
    head_ = (head_ + slots_.size() - 1) % slots_.size();
    --size_;
    // The machine now sits exactly where the snapshot was taken, so count that
    // frame as already spent rather than snapshotting the same point twice.
    counter_ = 1 % interval_;
    return gb.load_state(slots_[head_]);
}

void Rewind::clear() {
    head_ = 0;
    size_ = 0;
    counter_ = 0;
    for (std::vector<u8>& slot : slots_) {
        slot.clear();
    }
}

std::size_t Rewind::bytes() const {
    std::size_t total = 0;
    for (const std::vector<u8>& slot : slots_) {
        total += slot.capacity();
    }
    return total;
}

}  // namespace gb
