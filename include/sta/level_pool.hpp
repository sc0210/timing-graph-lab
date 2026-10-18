#pragma once
// Fixed worker pool for level-synchronous propagation (plan §6.6).
//
// Lifecycle: threads are created in the constructor and joined in the destructor;
// run() reuses them, so thread startup cost is measured separately ("pool_start").
// Within a level each worker owns a disjoint contiguous slice of nodes, so every
// node is written by exactly one thread; a std::barrier separates levels, which
// also orders all writes of level k before any read in level k+1 (no data race).
// Levels smaller than kGrain * threads use fewer workers, but every worker still
// passes every barrier, so per-level synchronization cost is part of the measurement.
//
// Barrier kinds (an experimental variable for plan Experiment D):
//   Block - std::barrier; waiting threads may sleep in the kernel (futex): cheap on CPU,
//           but a wake-up costs microseconds per level
//   Spin  - sense-reversing spin barrier that busy-waits briefly, then yields; much
//           cheaper per level on dedicated cores, wasteful if threads exceed cores
#include <atomic>
#include <barrier>
#include <functional>
#include <span>
#include <thread>
#include <vector>

#include "graph/traversal.hpp"

namespace tgl {

class SpinBarrier {
 public:
  explicit SpinBarrier(int n) : n_(n), count_(n) {}
  void arrive_and_wait() {
    const unsigned gen = gen_.load(std::memory_order_acquire);
    if (count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      count_.store(n_, std::memory_order_relaxed);
      gen_.fetch_add(1, std::memory_order_release);  // publishes the reset and all prior writes
      return;
    }
    for (unsigned spins = 0; gen_.load(std::memory_order_acquire) == gen; ++spins) {
      if (spins < 4096) {
#if defined(__x86_64__) || defined(__i386__)
        __builtin_ia32_pause();
#endif
      } else {
        std::this_thread::yield();
      }
    }
  }

 private:
  const int n_;
  std::atomic<int> count_;
  std::atomic<unsigned> gen_{0};
};

enum class BarrierKind { Block, Spin };

class LevelPool {
 public:
  static constexpr std::size_t kGrain = 256;  // minimum nodes per worker slice

  explicit LevelPool(int threads, BarrierKind kind = BarrierKind::Spin)
      : n_(threads < 1 ? 1 : threads), kind_(kind), block_(n_), spin_(n_) {
    for (int id = 1; id < n_; ++id) workers_.emplace_back([this, id] { worker_loop(id); });
  }
  ~LevelPool() {
    if (n_ > 1) {
      stop_ = true;
      wait();  // release workers waiting for a job
    }
    for (auto& t : workers_) t.join();
  }
  LevelPool(const LevelPool&) = delete;
  LevelPool& operator=(const LevelPool&) = delete;

  int size() const { return n_; }
  BarrierKind barrier_kind() const { return kind_; }

  // Calls fn(slice) for all nodes, level by level (high to low if reverse).
  void run(const Levels& levels, bool reverse, const std::function<void(std::span<const NodeId>)>& fn) {
    // With no levels there is no barrier to hold the caller until the workers have read the
    // job, so a woken worker could still dereference `levels` after run() returned (found by
    // fuzz_engine on an empty graph). Nothing to do, so do not wake the workers at all.
    if (levels.num_levels() == 0) return;
    levels_ = &levels;
    reverse_ = reverse;
    fn_ = fn;            // by value: the caller's callable is often a temporary
    if (n_ > 1) wait();  // publish the job
    work(0);
  }

 private:
  void worker_loop(int id) {
    for (;;) {
      wait();
      if (stop_) return;
      work(id);
    }
  }

  void work(int id) {
    // Read the job once. The caller stays in run() until the first level's barrier, so the
    // referenced Levels and fn are alive while used; after the last barrier only locals are
    // touched, because run() may then return and publish the next job.
    const Levels& L = *levels_;
    const bool reverse = reverse_;
    const auto& fn = fn_;
    const std::size_t nl = L.num_levels();
    for (std::size_t i = 0; i < nl; ++i) {
      auto lv = L.level(reverse ? nl - 1 - i : i);
      const std::size_t parts = std::min<std::size_t>(n_, (lv.size() + kGrain - 1) / kGrain);
      if (static_cast<std::size_t>(id) < parts) {
        const std::size_t b = lv.size() * id / parts, e = lv.size() * (id + 1) / parts;
        fn(lv.subspan(b, e - b));
      }
      if (n_ > 1) wait();
    }
  }

  void wait() {
    if (kind_ == BarrierKind::Spin)
      spin_.arrive_and_wait();
    else
      block_.arrive_and_wait();
  }

  int n_;
  BarrierKind kind_;
  std::barrier<> block_;
  SpinBarrier spin_;
  std::vector<std::thread> workers_;
  const Levels* levels_ = nullptr;
  bool reverse_ = false;
  std::function<void(std::span<const NodeId>)> fn_;
  bool stop_ = false;
};

}  // namespace tgl
