#pragma once

#include <cstddef>
#include <vector>

#include "core/types.h"

namespace gb {

class Gb;

// A fixed-size ring of periodic save states. Slots keep their allocations when
// they are overwritten, so a session that runs for hours never grows the heap
// past the configured capacity.
class Rewind {
  public:
    static constexpr std::size_t kDefaultInterval = 30;
    static constexpr std::size_t kDefaultCapacity = 600;

    Rewind() : Rewind(kDefaultInterval, kDefaultCapacity) {}
    Rewind(std::size_t interval_frames, std::size_t capacity);

    // Both drop whatever is buffered; the ring geometry changes underneath.
    void set_interval(std::size_t frames);
    void set_capacity(std::size_t snapshots);

    std::size_t interval() const { return interval_; }
    std::size_t capacity() const { return slots_.size(); }

    // Called once per frame by the owner; snapshots every interval() calls.
    void capture(Gb& gb);

    // Restores and drops the newest snapshot. False when nothing is buffered.
    bool step_back(Gb& gb);

    void clear();
    std::size_t size() const { return size_; }
    std::size_t bytes() const;

  private:
    std::vector<std::vector<u8>> slots_;
    std::size_t interval_ = kDefaultInterval;
    std::size_t head_ = 0;
    std::size_t size_ = 0;
    std::size_t counter_ = 0;
};

}  // namespace gb
