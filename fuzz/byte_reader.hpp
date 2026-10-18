#pragma once
// Consumes a fuzzer's byte string as a sequence of small decisions. Reads past the end
// return zero, so every input, including the empty one, decodes to a valid test case.
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace tgl::fuzz {

class ByteReader {
 public:
  ByteReader(const std::uint8_t* data, std::size_t size) : p_(data), n_(size) {}
  bool empty() const { return i_ >= n_; }
  std::uint8_t byte() { return i_ < n_ ? p_[i_++] : 0; }
  std::uint32_t u32() {
    std::uint32_t v = 0;
    for (int k = 0; k < 4; ++k) v = (v << 8) | byte();
    return v;
  }
  // Uniform-ish integer in [lo, hi].
  std::uint32_t range(std::uint32_t lo, std::uint32_t hi) { return hi <= lo ? lo : lo + u32() % (hi - lo + 1); }
  bool flag() { return byte() & 1; }

 private:
  const std::uint8_t* p_;
  std::size_t n_, i_ = 0;
};

}  // namespace tgl::fuzz
