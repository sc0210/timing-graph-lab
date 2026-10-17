#pragma once
// Reference implementations used as correctness oracles (plan §7.3).
// They share no code with the engine under test (no kahn_topo_sort, AdjacencyList,
// CsrGraph, eval_arrival/eval_required); they use only the raw EdgeList.
//   * path_oracle       - enumerates every path; exponential, tiny graphs only
//   * relaxation_oracle - Bellman-Ford-style fixed point, O(V*E); small/medium graphs
//   * reference_engine  - simple O(V+E) full recompute with its own topological sort;
//                         validated against the two above on small graphs, then used
//                         as the oracle for large graphs (plan §7.3, last paragraph)
#include <algorithm>
#include <deque>
#include <functional>
#include <numeric>
#include <optional>
#include <vector>

#include "graph/edge_list.hpp"
#include "sta/timing_state.hpp"

namespace tgl::oracle {

// Naive acyclicity check: repeatedly delete every node that has no remaining fanout.
inline bool is_acyclic(const EdgeList& el) {
  std::vector<std::uint8_t> alive(el.num_nodes, 1);
  for (bool progress = true; progress;) {
    std::vector<std::uint8_t> has_live_succ(el.num_nodes, 0);
    for (const Edge& e : el.edges)
      if (alive[e.src] && alive[e.dst]) has_live_succ[e.src] = 1;
    progress = false;
    for (NodeId v = 0; v < el.num_nodes; ++v)
      if (alive[v] && !has_live_succ[v]) alive[v] = 0, progress = true;
  }
  for (std::uint8_t a : alive)
    if (a) return false;
  return true;
}

// Returns nullopt for cyclic graphs, where the timing equations have no well-defined solution.
inline std::optional<TimingState> relaxation_oracle(const EdgeList& el, const TimingConstraints& c) {
  if (!is_acyclic(el)) return std::nullopt;
  const NodeId n = el.num_nodes;
  std::vector<std::uint8_t> has_pred(n, 0), has_succ(n, 0);
  for (const Edge& e : el.edges) {
    has_pred[e.dst] = 1;
    has_succ[e.src] = 1;
  }
  TimingState s;
  s.resize(n);
  for (NodeId v = 0; v < n; ++v) {
    if (!has_pred[v]) s.at[v] = c.a0(v);
    if (!has_succ[v]) s.rt[v] = c.r0(v);
  }
  bool converged = false;
  for (NodeId round = 0; round <= n && !converged; ++round) {
    converged = true;
    for (const Edge& e : el.edges) {
      if (s.at[e.src] + e.delay > s.at[e.dst]) {
        s.at[e.dst] = s.at[e.src] + e.delay;
        converged = false;
      }
      if (s.rt[e.dst] - e.delay < s.rt[e.src]) {
        s.rt[e.src] = s.rt[e.dst] - e.delay;
        converged = false;
      }
    }
  }
  if (!converged) return std::nullopt;
  for (NodeId v = 0; v < n; ++v) s.slack[v] = s.rt[v] - s.at[v];
  return s;
}

// AT(v) = max over all source->v paths of a0(source) + w1 + w2 + ...  (summed from the source)
// RT(v) = min over all v->sink paths of r0(sink) - wk - ... - w1      (summed from the sink)
// Accumulating in the same direction as the DP keeps the results bit-identical: IEEE
// rounding is monotone, so max/min commute with each rounded addition.
inline TimingState path_oracle(const EdgeList& el, const TimingConstraints& c) {
  const NodeId n = el.num_nodes;
  std::vector<std::vector<const Edge*>> out(n), in(n);
  for (const Edge& e : el.edges) {
    out[e.src].push_back(&e);
    in[e.dst].push_back(&e);
  }
  TimingState s;
  s.resize(n);
  std::function<void(NodeId, Delay)> fwd = [&](NodeId v, Delay at) {
    s.at[v] = std::max(s.at[v], at);
    for (const Edge* e : out[v]) fwd(e->dst, at + e->delay);
  };
  std::function<void(NodeId, Delay)> bwd = [&](NodeId v, Delay rt) {
    s.rt[v] = std::min(s.rt[v], rt);
    for (const Edge* e : in[v]) bwd(e->src, rt - e->delay);
  };
  for (NodeId v = 0; v < n; ++v) {
    if (in[v].empty()) fwd(v, c.a0(v));
    if (out[v].empty()) bwd(v, c.r0(v));
  }
  for (NodeId v = 0; v < n; ++v) s.slack[v] = s.rt[v] - s.at[v];
  return s;
}

// Independent topological sort (stable by node id via a deque of ready nodes, edge
// indices grouped with std::stable_sort) followed by per-node recomputation from the edge list.
inline std::optional<TimingState> reference_engine(const EdgeList& el, const TimingConstraints& c) {
  const NodeId n = el.num_nodes;
  std::vector<std::size_t> by_src(el.num_edges()), by_dst(el.num_edges());
  std::iota(by_src.begin(), by_src.end(), std::size_t{0});
  std::iota(by_dst.begin(), by_dst.end(), std::size_t{0});
  std::stable_sort(by_src.begin(), by_src.end(), [&](auto a, auto b) { return el.edges[a].src < el.edges[b].src; });
  std::stable_sort(by_dst.begin(), by_dst.end(), [&](auto a, auto b) { return el.edges[a].dst < el.edges[b].dst; });
  std::vector<std::size_t> out_begin(n + std::size_t{1}, 0), in_begin(n + std::size_t{1}, 0);
  for (const Edge& e : el.edges) ++out_begin[e.src + std::size_t{1}], ++in_begin[e.dst + std::size_t{1}];
  std::partial_sum(out_begin.begin(), out_begin.end(), out_begin.begin());
  std::partial_sum(in_begin.begin(), in_begin.end(), in_begin.begin());

  std::vector<std::size_t> missing(n);
  std::deque<NodeId> ready;
  for (NodeId v = 0; v < n; ++v)
    if ((missing[v] = in_begin[v + 1] - in_begin[v]) == 0) ready.push_back(v);
  std::vector<NodeId> order;
  order.reserve(n);
  while (!ready.empty()) {
    const NodeId u = ready.front();
    ready.pop_front();
    order.push_back(u);
    for (std::size_t i = out_begin[u]; i < out_begin[u + 1]; ++i)
      if (--missing[el.edges[by_src[i]].dst] == 0) ready.push_back(el.edges[by_src[i]].dst);
  }
  if (order.size() != n) return std::nullopt;

  TimingState s;
  s.resize(n);
  for (NodeId v : order) {
    if (in_begin[v] == in_begin[v + 1]) {
      s.at[v] = c.a0(v);
      continue;
    }
    for (std::size_t i = in_begin[v]; i < in_begin[v + 1]; ++i) {
      const Edge& e = el.edges[by_dst[i]];
      s.at[v] = std::max(s.at[v], s.at[e.src] + e.delay);
    }
  }
  for (auto it = order.rbegin(); it != order.rend(); ++it) {
    const NodeId v = *it;
    if (out_begin[v] == out_begin[v + 1]) {
      s.rt[v] = c.r0(v);
      continue;
    }
    for (std::size_t i = out_begin[v]; i < out_begin[v + 1]; ++i) {
      const Edge& e = el.edges[by_src[i]];
      s.rt[v] = std::min(s.rt[v], s.rt[e.dst] - e.delay);
    }
  }
  for (NodeId v = 0; v < n; ++v) s.slack[v] = s.rt[v] - s.at[v];
  return s;
}

}  // namespace tgl::oracle
