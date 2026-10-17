#pragma once
#include <chrono>
#include <cstdint>

namespace tgl::bench {

class Stopwatch {
 public:
  Stopwatch() : t0_(std::chrono::steady_clock::now()) {}
  std::int64_t elapsed_ns() const {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0_).count();
  }

 private:
  std::chrono::steady_clock::time_point t0_;
};

// Prevents the optimizer from discarding a computed value.
template <class T>
inline void do_not_optimize(const T& v) {
  asm volatile("" : : "g"(&v) : "memory");
}

}  // namespace tgl::bench
