#pragma once

#include <array>
#include <cstring>
#include <vector>

#include "core/types.h"

namespace gb {

// Save states are written with explicit little-endian scalar encoding rather
// than by copying structs, so a state produced by GCC loads byte-identically
// under Clang and MSVC. Rewind uses the same path.

class WriteArchive {
  public:
    static constexpr bool kLoading = false;

    explicit WriteArchive(std::vector<u8>& out) : out_(&out) {}

    void operator()(bool& v) { put(static_cast<u8>(v ? 1 : 0)); }
    void operator()(u8& v) { put(v); }
    void operator()(i8& v) { put(static_cast<u8>(v)); }
    void operator()(u16& v) { raw(v, 2); }
    void operator()(i16& v) { raw(static_cast<u16>(v), 2); }
    void operator()(u32& v) { raw(v, 4); }
    void operator()(i32& v) { raw(static_cast<u32>(v), 4); }
    void operator()(u64& v) { raw(v, 8); }
    void operator()(i64& v) { raw(static_cast<u64>(v), 8); }

    template <typename T, std::size_t N>
    void operator()(std::array<T, N>& a) {
        for (auto& e : a) {
            (*this)(e);
        }
    }

    void bytes(const void* p, std::size_t n) {
        const auto* b = static_cast<const u8*>(p);
        out_->insert(out_->end(), b, b + n);
    }

    bool ok() const { return true; }

  private:
    void put(u8 v) { out_->push_back(v); }

    void raw(u64 v, int n) {
        for (int i = 0; i < n; ++i) {
            put(static_cast<u8>((v >> (8 * i)) & 0xFF));
        }
    }

    std::vector<u8>* out_;
};

class ReadArchive {
  public:
    static constexpr bool kLoading = true;

    ReadArchive(const u8* data, std::size_t size) : data_(data), size_(size) {}

    void operator()(bool& v) { v = get() != 0; }
    void operator()(u8& v) { v = get(); }
    void operator()(i8& v) { v = static_cast<i8>(get()); }
    void operator()(u16& v) { v = static_cast<u16>(raw(2)); }
    void operator()(i16& v) { v = static_cast<i16>(raw(2)); }
    void operator()(u32& v) { v = static_cast<u32>(raw(4)); }
    void operator()(i32& v) { v = static_cast<i32>(raw(4)); }
    void operator()(u64& v) { v = raw(8); }
    void operator()(i64& v) { v = static_cast<i64>(raw(8)); }

    template <typename T, std::size_t N>
    void operator()(std::array<T, N>& a) {
        for (auto& e : a) {
            (*this)(e);
        }
    }

    void bytes(void* p, std::size_t n) {
        if (pos_ + n > size_) {
            ok_ = false;
            std::memset(p, 0, n);
            return;
        }
        std::memcpy(p, data_ + pos_, n);
        pos_ += n;
    }

    bool ok() const { return ok_; }

  private:
    u8 get() {
        if (pos_ >= size_) {
            ok_ = false;
            return 0;
        }
        return data_[pos_++];
    }

    u64 raw(int n) {
        u64 v = 0;
        for (int i = 0; i < n; ++i) {
            v |= static_cast<u64>(get()) << (8 * i);
        }
        return v;
    }

    const u8* data_;
    std::size_t size_;
    std::size_t pos_ = 0;
    bool ok_ = true;
};

}  // namespace gb

// Component visit() bodies live in their .cpp; this pins the two archive types.
#define GB_INSTANTIATE_VISIT(Type)                        \
    template void Type::visit<::gb::WriteArchive>(::gb::WriteArchive&); \
    template void Type::visit<::gb::ReadArchive>(::gb::ReadArchive&)
