#pragma once
// Backward min-plus Required propagation in reverse topological order (plan §3.3, §6.3),
// slack, and the full STA pipeline.
#include <span>
#include <vector>

#include "graph/traversal.hpp"
#include "sta/arrival.hpp"
#include "sta/level_pool.hpp"
#include "sta/timing_state.hpp"

namespace tgl {

template <class G>
inline Delay eval_required(const G& g, const std::vector<Delay>& rt, const TimingConstraints& c, NodeId v) {
  auto out = g.fanout(v);
  if (out.empty()) return c.r0(v);
  Delay m = kPosInf;
  for (const Arc& a : out) m = std::min(m, rt[a.node] - a.delay);
  return m;
}

template <class G>
void propagate_required(const G& g, std::span<const NodeId> topo_order, const TimingConstraints& c,
                        std::vector<Delay>& rt) {
  rt.resize(g.num_nodes());
  for (auto it = topo_order.rbegin(); it != topo_order.rend(); ++it) rt[*it] = eval_required(g, rt, c, *it);
}

// Every fanout of a level-k node is at a level > k, so levels are processed high to low.
template <class G>
void propagate_required_parallel(const G& g, const Levels& levels, const TimingConstraints& c,
                                 std::vector<Delay>& rt, LevelPool& pool) {
  rt.resize(g.num_nodes());
  pool.run(levels, true, [&](std::span<const NodeId> nodes) {
    for (NodeId v : nodes) rt[v] = eval_required(g, rt, c, v);
  });
}

inline void compute_slack(TimingState& s) {
  s.slack.resize(s.at.size());
  for (std::size_t v = 0; v < s.at.size(); ++v) s.slack[v] = s.rt[v] - s.at[v];
}

// Full STA: arrival, required and slack.
template <class G>
void run_full_sta(const G& g, std::span<const NodeId> topo_order, const TimingConstraints& c, TimingState& s) {
  propagate_arrival(g, topo_order, c, s.at);
  propagate_required(g, topo_order, c, s.rt);
  compute_slack(s);
}

}  // namespace tgl
