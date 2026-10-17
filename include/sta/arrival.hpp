#pragma once
// Forward max-plus Arrival propagation (plan §3.2, §6.2).
//   pull: AT(v) = max over fanin, nodes in topological order (needs the fanin index)
//   push: AT(v) initialized to a0 for sources and -inf otherwise, then each node in
//         topological order relaxes its fanout (the plan's pseudo-code)
// Both are exact: max() never rounds and each candidate AT(u)+w is the same sum,
// so pull, push and level-parallel results are bit-identical.
#include <span>
#include <vector>

#include "graph/traversal.hpp"
#include "sta/level_pool.hpp"
#include "sta/timing_state.hpp"

namespace tgl {

template <class G>
inline Delay eval_arrival(const G& g, const std::vector<Delay>& at, const TimingConstraints& c, NodeId v) {
  auto in = g.fanin(v);
  if (in.empty()) return c.a0(v);
  Delay m = kNegInf;
  for (const Arc& a : in) m = std::max(m, at[a.node] + a.delay);
  return m;
}

template <class G>
void propagate_arrival(const G& g, std::span<const NodeId> topo_order, const TimingConstraints& c,
                       std::vector<Delay>& at) {
  at.resize(g.num_nodes());
  for (NodeId v : topo_order) at[v] = eval_arrival(g, at, c, v);
}

template <class G>
void propagate_arrival_push(const G& g, std::span<const NodeId> topo_order, const TimingConstraints& c,
                            std::vector<Delay>& at) {
  const NodeId n = g.num_nodes();
  at.assign(n, kNegInf);
  for (NodeId v = 0; v < n; ++v)
    if (g.fanin(v).empty()) at[v] = c.a0(v);
  for (NodeId u : topo_order) {
    const Delay au = at[u];
    for (const Arc& a : g.fanout(u)) at[a.node] = std::max(at[a.node], au + a.delay);
  }
}

template <class G>
void propagate_arrival_parallel(const G& g, const Levels& levels, const TimingConstraints& c,
                                std::vector<Delay>& at, LevelPool& pool) {
  at.resize(g.num_nodes());
  pool.run(levels, false, [&](std::span<const NodeId> nodes) {
    for (NodeId v : nodes) at[v] = eval_arrival(g, at, c, v);
  });
}

}  // namespace tgl
