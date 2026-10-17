#include "graph/generators.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace tgl {

namespace {

std::uint64_t splitmix64(std::uint64_t& x) {
  std::uint64_t z = (x += 0x9E3779B97F4A7C15ULL);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}

std::uint64_t rotl(std::uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }

template <class T>
void shuffle_in_place(std::vector<T>& v, Rng& rng) {
  for (std::size_t i = v.size(); i > 1; --i) std::swap(v[i - 1], v[rng.uniform(0, i - 1)]);
}

std::uint32_t fanin_count(double mean, Rng& rng) {
  // Integer part always, fractional part with matching probability: E[count] = mean.
  const double fl = std::floor(mean);
  auto k = static_cast<std::uint32_t>(fl);
  const double frac = mean - fl;
  if (frac > 0 && rng.unit() < frac) ++k;
  return std::max<std::uint32_t>(k, 1);
}

// Splits [0, n) into `layers` contiguous, near-equal ranges.
std::vector<NodeId> layer_bounds(NodeId n, NodeId layers) {
  std::vector<NodeId> b(layers + 1);
  for (NodeId l = 0; l <= layers; ++l) b[l] = static_cast<NodeId>(std::uint64_t{n} * l / layers);
  return b;
}

// Each non-source node takes distinct preds sampled from layers [l - span, l - 1],
// with at least one from l - 1 so the layer structure is preserved.
void layered_edges(const GenParams& p, NodeId span, Rng& rng, EdgeList& el) {
  const NodeId L = p.layers ? p.layers : std::max<NodeId>(2, static_cast<NodeId>(std::sqrt(double(p.nodes))));
  if (L > p.nodes) throw std::invalid_argument("more layers than nodes");
  if (L < 2) throw std::invalid_argument("layered graphs need at least 2 layers");
  const auto b = layer_bounds(p.nodes, L);
  std::vector<NodeId> picked;
  for (NodeId l = 1; l < L; ++l) {
    const NodeId prev_lo = b[l - 1], prev_hi = b[l];
    const NodeId lo = b[l >= span ? l - span : 0];
    for (NodeId v = b[l]; v < b[l + 1]; ++v) {
      const std::uint32_t k = std::min<std::uint32_t>(fanin_count(p.fanout, rng), prev_hi - lo);
      picked.clear();
      picked.push_back(static_cast<NodeId>(rng.uniform(prev_lo, prev_hi - 1)));
      while (picked.size() < k) {
        const auto u = static_cast<NodeId>(rng.uniform(lo, prev_hi - 1));
        if (std::find(picked.begin(), picked.end(), u) == picked.end()) picked.push_back(u);
      }
      for (NodeId u : picked) el.edges.push_back({u, v, 0});
    }
  }
}

void random_dag_edges(const GenParams& p, Rng& rng, EdgeList& el) {
  if (p.nodes < 2) return;
  const auto max_edges = std::uint64_t{p.nodes} * (p.nodes - 1) / 2;
  const auto m = std::min<std::uint64_t>(static_cast<std::uint64_t>(p.fanout * p.nodes), max_edges);
  std::unordered_set<std::uint64_t> seen;
  seen.reserve(m * 2);
  while (el.edges.size() < m) {
    const auto v = static_cast<NodeId>(rng.uniform(1, p.nodes - 1));
    const auto u = static_cast<NodeId>(rng.uniform(0, v - 1));
    if (seen.insert((std::uint64_t{u} << 32) | v).second) el.edges.push_back({u, v, 0});
  }
}

}  // namespace

Rng::Rng(std::uint64_t seed) {
  for (auto& s : s_) s = splitmix64(seed);
}

std::uint64_t Rng::next() {
  const std::uint64_t result = rotl(s_[1] * 5, 7) * 9;
  const std::uint64_t t = s_[1] << 17;
  s_[2] ^= s_[0];
  s_[3] ^= s_[1];
  s_[1] ^= s_[2];
  s_[0] ^= s_[3];
  s_[2] ^= t;
  s_[3] = rotl(s_[3], 45);
  return result;
}

std::uint64_t Rng::uniform(std::uint64_t lo, std::uint64_t hi) {
  const std::uint64_t range = hi - lo + 1;
  if (range == 0) return next();                                // full 64-bit range
  const std::uint64_t limit = UINT64_MAX - UINT64_MAX % range;  // rejection removes modulo bias
  std::uint64_t x;
  do x = next();
  while (x >= limit);
  return lo + x % range;
}

EdgeList generate(const GenParams& p) {
  if (!(p.delay_min >= 0) || p.delay_min > p.delay_max) throw std::invalid_argument("need 0 <= delay_min <= delay_max");
  if (p.fanout <= 0) throw std::invalid_argument("fanout must be > 0");
  if (p.delay_dist != "real" && p.delay_dist != "int") throw std::invalid_argument("delay_dist must be real or int");
  Rng rng(p.seed);
  EdgeList el;
  el.num_nodes = p.nodes;
  if (p.nodes == 0) return el;
  if (p.kind == "chain") {
    for (NodeId v = 1; v < p.nodes; ++v) el.edges.push_back({v - 1, v, 0});
  } else if (p.kind == "layered") {
    layered_edges(p, 1, rng, el);
  } else if (p.kind == "reconvergent") {
    layered_edges(p, std::max<NodeId>(p.span, 2), rng, el);
  } else if (p.kind == "random") {
    random_dag_edges(p, rng, el);
  } else {
    throw std::invalid_argument("unknown generator kind: " + p.kind);
  }
  if (p.delay_dist == "int") {
    const auto lo = static_cast<std::uint64_t>(std::ceil(p.delay_min));
    const auto hi = static_cast<std::uint64_t>(std::floor(p.delay_max));
    if (lo > hi) throw std::invalid_argument("no integer in [delay_min, delay_max]");
    for (Edge& e : el.edges) e.delay = static_cast<Delay>(rng.uniform(lo, hi));
  } else {
    for (Edge& e : el.edges) e.delay = p.delay_min + (p.delay_max - p.delay_min) * rng.unit();
  }
  if (p.shuffle) {
    std::vector<NodeId> perm(p.nodes);
    for (NodeId v = 0; v < p.nodes; ++v) perm[v] = v;
    shuffle_in_place(perm, rng);
    for (Edge& e : el.edges) {
      e.src = perm[e.src];
      e.dst = perm[e.dst];
    }
    shuffle_in_place(el.edges, rng);
  }
  return el;
}

}  // namespace tgl
