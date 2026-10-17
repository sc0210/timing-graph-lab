#pragma once
// Incremental STA after edge-delay changes on a fixed-topology CsrGraph (plan §6.4, §6.5).
//
// A delay change on edge (u, v) can change AT only for v and its transitive fanout,
// and RT only for u and its transitive fanin. Two strategies are provided so that
// "shrinking the recomputed region" and "shrinking work inside the region" can be
// measured separately:
//
//   Conservative - collect the whole reachable region from the changed edges
//                  (fanout cone for AT, fanin cone for RT), sort it by topological
//                  rank, and recompute every node in it from all of its inputs.
//   EarlyStop    - event-driven: a priority queue keyed by topological rank;
//                  a node's dependents are scheduled only if its value changed.
//
// Both recompute a node from ALL of its fanin (fanout for RT), never just the
// modified edge, because another path may still determine the max/min.
#include <cstdint>
#include <queue>
#include <vector>

#include "graph/csr_graph.hpp"
#include "sta/required.hpp"

namespace tgl {

enum class IncMode { Conservative, EarlyStop };

struct IncrementalStats {
  std::uint64_t updated_edges = 0;   // set_edge_delay calls that changed a delay
  std::uint64_t affected_fwd = 0;    // nodes in the AT region (Conservative only; see affected_region())
  std::uint64_t affected_bwd = 0;    // nodes in the RT region (Conservative only)
  std::uint64_t forward_evals = 0;   // nodes whose AT was recomputed
  std::uint64_t backward_evals = 0;  // nodes whose RT was recomputed
  std::uint64_t at_changed = 0;
  std::uint64_t rt_changed = 0;
  std::uint64_t changed_nodes = 0;   // nodes whose AT or RT (hence slack) changed
};

struct AffectedRegion {
  std::uint64_t fwd = 0, bwd = 0, either = 0;
};

class IncrementalTimer {
 public:
  // Runs a full STA to initialize. `g` must outlive the timer and stay a DAG.
  IncrementalTimer(CsrGraph& g, TimingConstraints c);

  // Changes one edge delay; the timing is stale until update() is called.
  void set_edge_delay(EdgeId e, Delay d);
  // Size of the region the pending changes could affect (does not modify state).
  AffectedRegion affected_region();
  // Propagates all pending changes.
  IncrementalStats update(IncMode mode = IncMode::EarlyStop);
  // Recomputes everything from scratch with the cached topological order (Full Recompute baseline).
  void full_recompute();
  // Restores every delay from `el`, drops pending changes and recomputes (per-repetition reset).
  void reset(const EdgeList& el);

  const TimingState& state() const { return s_; }
  const std::vector<NodeId>& topo_order() const { return order_; }

 private:
  void collect_cone(const std::vector<NodeId>& seeds, bool forward, std::vector<NodeId>& out);
  bool set_at(NodeId v, Delay nv, IncrementalStats& st);
  bool set_rt(NodeId v, Delay nv, IncrementalStats& st);
  void mark_changed(NodeId v, IncrementalStats& st);

  CsrGraph& g_;
  TimingConstraints c_;
  TimingState s_;
  std::vector<NodeId> order_;
  std::vector<std::uint32_t> rank_;
  std::vector<NodeId> seeds_fwd_, seeds_bwd_;  // pending: dst / src of changed edges
  std::uint64_t pending_edges_ = 0;
  // Scratch flags, all zero between calls.
  std::vector<std::uint8_t> mark_, in_queue_, changed_;
  std::vector<NodeId> changed_list_;
};

}  // namespace tgl
