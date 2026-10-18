#pragma once
// oneTBB propagation engines (optional; built when CMake finds TBB, see TGL_TBB).
//
// Two engines, both bit-identical to the serial pull engine (every node is written by
// exactly one task and evaluates the same max/min over the same candidates):
//
//   level_parallel_tbb - same level schedule as LevelPool, but each level is a
//                        tbb::parallel_for with work stealing; the implicit join at the
//                        end of each parallel_for replaces the explicit barrier. Levels
//                        smaller than kGrain run inline on the calling thread.
//   dag_parallel_tbb   - no levels: a node becomes ready as soon as its last predecessor
//                        (fanin for arrival, fanout for required) finishes, counted with
//                        atomics and scheduled through a parallel_for_each feeder. The
//                        acq_rel decrement orders a predecessor's write before the read.
//
// TbbPool fixes the concurrency to `threads` (global_control + task_arena), so thread
// counts are comparable with LevelPool; constructing it is timed as pool_start.
#include <tbb/blocked_range.h>
#include <tbb/global_control.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_for_each.h>
#include <tbb/task_arena.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "graph/traversal.hpp"
#include "sta/arrival.hpp"
#include "sta/required.hpp"
#include "sta/timing_state.hpp"

namespace tgl {

class TbbPool {
 public:
  static constexpr std::size_t kGrain = 256;  // same minimum slice as LevelPool

  explicit TbbPool(int threads)
      : n_(threads < 1 ? 1 : threads), gc_(tbb::global_control::max_allowed_parallelism, n_), arena_(n_) {
    arena_.initialize();
    // Start the worker threads now so that start-up cost lands in pool_start, not in the first run.
    arena_.execute([] { tbb::parallel_for(0, 1 << 12, [](int) {}); });
  }
  TbbPool(const TbbPool&) = delete;
  TbbPool& operator=(const TbbPool&) = delete;

  int size() const { return n_; }

  template <class F>
  void execute(F&& f) {
    arena_.execute(std::forward<F>(f));
  }

  // Calls fn(slice) for all nodes, level by level (high to low if reverse).
  template <class F>
  void run_levels(const Levels& L, bool reverse, const F& fn) {
    arena_.execute([&] {
      const std::size_t nl = L.num_levels();
      for (std::size_t i = 0; i < nl; ++i) {
        auto lv = L.level(reverse ? nl - 1 - i : i);
        if (n_ == 1 || lv.size() <= kGrain) {
          fn(lv);
          continue;
        }
        tbb::parallel_for(tbb::blocked_range<std::size_t>(0, lv.size(), kGrain),
                          [&](const tbb::blocked_range<std::size_t>& r) { fn(lv.subspan(r.begin(), r.size())); });
      }
    });
  }

 private:
  int n_;
  tbb::global_control gc_;
  tbb::task_arena arena_;
};

// Static part of the dependency-driven schedule, built once per graph (timed as dag_prepare).
struct DagSchedule {
  std::vector<std::uint32_t> fanin_count, fanout_count;
  std::vector<NodeId> sources, sinks;

  std::size_t memory_bytes() const {
    return (fanin_count.capacity() + fanout_count.capacity()) * sizeof(std::uint32_t) +
           (sources.capacity() + sinks.capacity()) * sizeof(NodeId);
  }
};

template <class G>
DagSchedule make_dag_schedule(const G& g) {
  const NodeId n = g.num_nodes();
  DagSchedule s;
  s.fanin_count.resize(n);
  s.fanout_count.resize(n);
  for (NodeId v = 0; v < n; ++v) {
    s.fanin_count[v] = static_cast<std::uint32_t>(g.fanin(v).size());
    s.fanout_count[v] = static_cast<std::uint32_t>(g.fanout(v).size());
    if (s.fanin_count[v] == 0) s.sources.push_back(v);
    if (s.fanout_count[v] == 0) s.sinks.push_back(v);
  }
  return s;
}

namespace detail {

inline constexpr NodeId kNoNode = ~NodeId{0};

// Runs eval(v) for every node once all of v's predecessors (next(v) are its successors) are done.
template <class Next, class Eval>
void run_dag(TbbPool& pool, const std::vector<std::uint32_t>& counts, const std::vector<NodeId>& seeds,
             const Next& next, const Eval& eval) {
  const std::size_t n = counts.size();
  auto pending = std::make_unique<std::atomic<std::uint32_t>[]>(n);
  pool.execute([&] {
    tbb::parallel_for(tbb::blocked_range<std::size_t>(0, n, 4096), [&](const tbb::blocked_range<std::size_t>& r) {
      for (std::size_t v = r.begin(); v < r.end(); ++v) pending[v].store(counts[v], std::memory_order_relaxed);
    });
    tbb::parallel_for_each(seeds.begin(), seeds.end(), [&](NodeId v, tbb::feeder<NodeId>& feeder) {
      // Follow one ready successor directly instead of feeding it: keeps chains on one
      // thread without a task per node.
      for (;;) {
        eval(v);
        NodeId cont = kNoNode;
        for (const Arc& a : next(v))
          if (pending[a.node].fetch_sub(1, std::memory_order_acq_rel) == 1) {
            if (cont == kNoNode)
              cont = a.node;
            else
              feeder.add(a.node);
          }
        if (cont == kNoNode) return;
        v = cont;
      }
    });
  });
}

}  // namespace detail

template <class G>
void propagate_arrival_tbb(const G& g, const Levels& levels, const TimingConstraints& c, std::vector<Delay>& at,
                           TbbPool& pool) {
  at.resize(g.num_nodes());
  pool.run_levels(levels, false, [&](std::span<const NodeId> nodes) {
    for (NodeId v : nodes) at[v] = eval_arrival(g, at, c, v);
  });
}

template <class G>
void propagate_required_tbb(const G& g, const Levels& levels, const TimingConstraints& c, std::vector<Delay>& rt,
                            TbbPool& pool) {
  rt.resize(g.num_nodes());
  pool.run_levels(levels, true, [&](std::span<const NodeId> nodes) {
    for (NodeId v : nodes) rt[v] = eval_required(g, rt, c, v);
  });
}

template <class G>
void propagate_arrival_dag_tbb(const G& g, const DagSchedule& s, const TimingConstraints& c, std::vector<Delay>& at,
                               TbbPool& pool) {
  at.resize(g.num_nodes());
  detail::run_dag(
      pool, s.fanin_count, s.sources, [&](NodeId v) { return g.fanout(v); },
      [&](NodeId v) { at[v] = eval_arrival(g, at, c, v); });
}

template <class G>
void propagate_required_dag_tbb(const G& g, const DagSchedule& s, const TimingConstraints& c, std::vector<Delay>& rt,
                                TbbPool& pool) {
  rt.resize(g.num_nodes());
  detail::run_dag(
      pool, s.fanout_count, s.sinks, [&](NodeId v) { return g.fanin(v); },
      [&](NodeId v) { rt[v] = eval_required(g, rt, c, v); });
}

}  // namespace tgl
