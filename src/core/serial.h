#pragma once

#include <functional>

#include "core/state/serialize.h"
#include "core/types.h"

namespace gb {

class Bus;

// No link cable peer: transfers always shift in 0xFF. The byte sink exists
// because every Blargg test reports its result over the serial port.
class Serial {
  public:
    using ByteSink = std::function<void(u8)>;

    void attach(Bus* bus) { bus_ = bus; }
    void reset();

    void set_sink(ByteSink sink) { sink_ = std::move(sink); }

    void tick(u64 tcycles);

    u8 read(u16 addr) const;
    void write(u16 addr, u8 value);

    template <typename Ar>
    void visit(Ar& ar);

  private:
    Bus* bus_ = nullptr;
    ByteSink sink_;
    u8 sb_ = 0;
    u8 sc_ = 0;
    u8 bits_left_ = 0;
    // SB is destroyed by the 0xFF an unplugged cable shifts back in, so the
    // byte the ROM meant to send is latched when the transfer starts.
    u8 out_byte_ = 0;
    u16 shift_timer_ = 0;
};

}  // namespace gb
