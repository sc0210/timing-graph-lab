#pragma once
// Graph traversals over any representation exposing num_nodes()/fanout()/fanin().
//   * kahn_topo_sort  - baseline order for DAG timing propagation (also detects cycles)
//   * dfs_topo_sort   - reverse DFS post-order, alternative topological order
//   * bfs_order       - multi-source BFS from sources; NOT a valid timing order in general (plan §3.4)
//   * levelize        - hop level from sources; buckets used by level-parallel propagation
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include "graph/edge_list.hpp"

namespace tgl {

struct TopoResult {
  std::vector<NodeId> order;
  bool is_dag = false;
  std::uint64_t work = 0;     // arcs scanned
  std::int64_t init_ns = 0;   // Kahn only: indegree initialization (plan §6.1)
  std::size_t aux_bytes = 0;  // auxiliary memory beyond the output order (indegree, colors, stack)
};

template <class G>
TopoResult kahn_topo_sort(const G& g) {
  const NodeId n = g.num_nodes();
  TopoResult r;
  const auto t0 = std::chrono::steady_clock::now();
  r.order.reserve(n);
  std::vector<std::uint32_t> indeg(n);
  for (NodeId v = 0; v < n; ++v) indeg[v] = static_cast<std::uint32_t>(g.fanin(v).size());
  r.init_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count();
  r.aux_bytes = indeg.capacity() * sizeof(std::uint32_t);
  for (NodeId v = 0; v < n; ++v)
    if (indeg[v] == 0) r.order.push_back(v);
  // r.order doubles as the FIFO queue.
  for (std::size_t head = 0; head < r.order.size(); ++head) {
    for (const Arc& a : g.fanout(r.order[head])) {
      ++r.work;
      if (--indeg[a.node] == 0) r.order.push_back(a.node);
    }
  }
  r.is_dag = r.order.size() == n;
  return r;
}

template <class G>
TopoResult dfs_topo_sort(const G& g) {
  enum : std::uint8_t { kWhite, kGray, kBlack };
  const NodeId n = g.num_nodes();
  TopoResult r;
  r.is_dag = true;
  r.order.reserve(n);
  std::vector<std::uint8_t> color(n, kWhite);
  std::vector<std::pair<NodeId, std::uint32_t>> stack;  // (node, next fanout index)
  for (NodeId root = 0; root < n; ++root) {
    if (color[root] != kWhite) continue;
    color[root] = kGray;
    stack.push_back({root, 0});
    while (!stack.empty()) {
      auto& [v, i] = stack.back();
      auto out = g.fanout(v);
      if (i < out.size()) {
        const NodeId w = out[i++].node;
        ++r.work;
        if (color[w] == kWhite) {
          color[w] = kGray;
          stack.push_back({w, 0});
        } else if (color[w] == kGray) {
          r.is_dag = false;  // back edge
        }
      } else {
        color[v] = kBlack;
        r.order.push_back(v);
        stack.pop_back();
      }
    }
  }
  r.aux_bytes = color.capacity() + stack.capacity() * sizeof(stack[0]);
  std::reverse(r.order.begin(), r.order.end());
  return r;
}

template <class G>
TopoResult bfs_order(const G& g) {
  const NodeId n = g.num_nodes();
  TopoResult r;
  r.order.reserve(n);
  std::vector<std::uint8_t> seen(n, 0);
  for (NodeId v = 0; v < n; ++v)
    if (g.fanin(v).empty()) {
      seen[v] = 1;
      r.order.push_back(v);
    }
  for (std::size_t head = 0; head < r.order.size(); ++head) {
    for (const Arc& a : g.fanout(r.order[head])) {
      ++r.work;
      if (!seen[a.node]) {
        seen[a.node] = 1;
        r.order.push_back(a.node);
      }
    }
  }
  r.is_dag = false;  // BFS visit order carries no DAG guarantee
  r.aux_bytes = seen.capacity();
  return r;
}

// Nodes grouped by level: level(v) = 0 for sources, else 1 + max level of fanin.
// All fanin of a level-k node lies in levels < k, so each level can be evaluated in parallel.
struct Levels {
  std::vector<std::uint32_t> offsets;  // size = num_levels + 1
  std::vector<NodeId> nodes;
  std::size_t num_levels() const { return offsets.size() - 1; }
  std::size_t width(std::size_t k) const { return offsets[k + 1] - offsets[k]; }
  std::span<const NodeId> level(std::size_t k) const {
    return {nodes.data() + offsets[k], nodes.data() + offsets[k + 1]};
  }
};

template <class G>
Levels levelize(const G& g, std::span<const NodeId> topo_order) {
  const NodeId n = g.num_nodes();
  std::vector<std::uint32_t> level(n, 0);
  std::uint32_t max_level = 0;
  for (NodeId v : topo_order) {
    std::uint32_t lv = 0;
    for (const Arc& a : g.fanin(v)) lv = std::max(lv, level[a.node] + 1);
    level[v] = lv;
    max_level = std::max(max_level, lv);
  }
  Levels L;
  L.offsets.assign(n == 0 ? 1 : max_level + 2, 0);
  for (NodeId v = 0; v < n; ++v) ++L.offsets[level[v] + 1];
  for (std::size_t k = 1; k < L.offsets.size(); ++k) L.offsets[k] += L.offsets[k - 1];
  L.nodes.resize(n);
  std::vector<std::uint32_t> cursor(L.offsets.begin(), L.offsets.end() - 1);
  for (NodeId v : topo_order) L.nodes[cursor[level[v]]++] = v;
  return L;
}

// True iff order is a permutation of all nodes and every edge goes forward in it.
template <class G>
bool is_topological_order(const G& g, std::span<const NodeId> order) {
  const NodeId n = g.num_nodes();
  if (order.size() != n) return false;
  std::vector<std::uint32_t> pos(n, UINT32_MAX);
  for (std::uint32_t i = 0; i < n; ++i) {
    if (order[i] >= n || pos[order[i]] != UINT32_MAX) return false;
    pos[order[i]] = i;
  }
  for (NodeId v = 0; v < n; ++v)
    for (const Arc& a : g.fanout(v))
      if (pos[v] >= pos[a.node]) return false;
  return true;
}

}  // namespace tgl
