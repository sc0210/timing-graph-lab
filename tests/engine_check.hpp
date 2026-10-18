#pragma once
// Runs every engine variant on one graph and compares each with the independent reference
// engine. Shared by the stress tests, the trace replayer and the fuzzers, so that every
// new testbench automatically covers every engine.
//
// Variants: Adjacency List and CSR (pull over Kahn order), CSR over DFS order, push arrival,
// LevelPool with spin and blocking barriers, the oneTBB engines when compiled in, and the
// incremental timer (conservative and early-stop) after a reset.
#include <memory>
#include <optional>
#include <sstream>
#include <string>

#include "graph/adjacency_list.hpp"
#include "graph/csr_graph.hpp"
#include "graph/traversal.hpp"
#include "sta/arrival.hpp"
#include "sta/compare.hpp"
#include "sta/incremental.hpp"
#include "sta/level_pool.hpp"
#include "sta/oracle.hpp"
#include "sta/required.hpp"
#ifdef TGL_HAVE_TBB
#include "sta/tbb_parallel.hpp"
#endif

namespace tgl::test {

// Thread pools are expensive to create; callers that check many graphs (fuzzers) keep one.
struct EnginePools {
  LevelPool spin{2, BarrierKind::Spin};
  LevelPool block{3, BarrierKind::Block};
#ifdef TGL_HAVE_TBB
  TbbPool tbb{2};
#endif
};

struct CheckOptions {
  Tolerance tol{};
  bool parallel = true;  // level-parallel engines pay one barrier per level: skip for very deep graphs
};

namespace detail {
inline std::string describe(const char* engine, const CompareResult& r) {
  std::ostringstream o;
  o << engine << ": " << r.mismatches << " mismatching value(s), " << r.non_finite << " non-finite, max |err| "
    << r.max_abs_err;
  return o.str();
}
}  // namespace detail

// Returns an empty string when every variant matches the reference, else a description of
// the first failure. A cyclic graph is reported as a failure (callers should not pass one).
inline std::string check_all_engines(const EdgeList& el, const TimingConstraints& c, EnginePools& pools,
                                     const CheckOptions& opt = {}) {
  const std::optional<TimingState> ref = oracle::reference_engine(el, c);
  if (!ref) return "reference engine: graph has a cycle";
  auto check = [&](const char* name, const TimingState& s) -> std::string {
    const CompareResult r = compare_timing(s, *ref, opt.tol);
    return r.pass ? std::string() : detail::describe(name, r);
  };
  auto full = [&](const auto& g, std::span<const NodeId> order) {
    TimingState s;
    s.resize(g.num_nodes());
    run_full_sta(g, order, c, s);
    return s;
  };

  const AdjacencyList list(el);
  CsrGraph csr(el);
  const TopoResult kahn = kahn_topo_sort(csr);
  const TopoResult dfs = dfs_topo_sort(csr);
  if (!kahn.is_dag || !dfs.is_dag) return "topological sort reported a cycle on an acyclic graph";
  if (!is_topological_order(csr, kahn.order) || !is_topological_order(csr, dfs.order))
    return "invalid topological order";

  if (auto e = check("list/pull", full(list, kahn_topo_sort(list).order)); !e.empty()) return e;
  if (auto e = check("csr/pull", full(csr, kahn.order)); !e.empty()) return e;
  if (auto e = check("csr/pull_dfs_order", full(csr, dfs.order)); !e.empty()) return e;
  {
    TimingState s = full(csr, kahn.order);
    propagate_arrival_push(list, kahn.order, c, s.at);
    if (auto e = check("list/push", s); !e.empty()) return e;
  }
  if (opt.parallel) {
    const Levels levels = levelize(csr, kahn.order);
    for (LevelPool* pool : {&pools.spin, &pools.block}) {
      TimingState s;
      s.resize(csr.num_nodes());
      propagate_arrival_parallel(csr, levels, c, s.at, *pool);
      propagate_required_parallel(list, levels, c, s.rt, *pool);
      compute_slack(s);
      if (auto e = check(pool == &pools.spin ? "level_parallel_spin" : "level_parallel_block", s); !e.empty()) return e;
    }
#ifdef TGL_HAVE_TBB
    {
      TimingState s;
      s.resize(csr.num_nodes());
      propagate_arrival_tbb(csr, levels, c, s.at, pools.tbb);
      propagate_required_tbb(csr, levels, c, s.rt, pools.tbb);
      compute_slack(s);
      if (auto e = check("level_parallel_tbb", s); !e.empty()) return e;
      const DagSchedule dag = make_dag_schedule(csr);
      propagate_arrival_dag_tbb(list, dag, c, s.at, pools.tbb);
      propagate_required_dag_tbb(list, dag, c, s.rt, pools.tbb);
      compute_slack(s);
      if (auto e = check("dag_parallel_tbb", s); !e.empty()) return e;
    }
#endif
  }
  {
    IncrementalTimer timer(csr, c);
    if (auto e = check("incremental/initial", timer.state()); !e.empty()) return e;
  }
  return {};
}

}  // namespace tgl::test
