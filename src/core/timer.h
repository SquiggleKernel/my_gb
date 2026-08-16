#pragma once

#include "core/state/serialize.h"
#include "core/types.h"

namespace gb {

class Bus;

// DIV is the top 8 bits of a 16-bit counter running at the master clock. TIMA
// increments on the falling edge of (counter bit N) AND (TAC enable), which is
// what makes writes to DIV/TAC able to tick the timer as a side effect.
class Timer {
  public:
    void attach(Bus* bus) { bus_ = bus; }
    void reset();

    void tick(u64 tcycles);

    u8 read(u16 addr) const;
    void write(u16 addr, u8 value);

    u16 div_counter() const { return counter_; }

    template <typename Ar>
    void visit(Ar& ar);

  private:
    void set_counter(u16 value);
    bool edge_input() const;
    void detect_edge(bool before);
    void step_one();

    Bus* bus_ = nullptr;
    u16 counter_ = 0;
    u8 tima_ = 0;
    u8 tma_ = 0;
    u8 tac_ = 0;
    // TIMA reads 0 for the four cycles between overflow and the TMA reload.
    u8 reload_delay_ = 0;
    bool reloaded_ = false;
};

}  // namespace gb
