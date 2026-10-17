#pragma once
// Deterministic synthetic DAG generators (plan Layer 2).
// Uses a self-contained xoshiro256** RNG (no std:: distributions, whose output differs
// between standard libraries), so (kind, nodes, seed, ...) gives bit-identical graphs everywhere.
//
//   chain        - 0 -> 1 -> ... -> n-1
//   layered      - edges only between adjacent layers
//   reconvergent - layered, but fanin may come from the previous `span` layers, so
//                  paths that fork reconverge several layers later
//   random       - random order; edges only from earlier to later nodes (acyclic by construction)
//
// `fanout` is the target average fanout. Non-source nodes draw their fanin count with
// mean `fanout` (layered/reconvergent), or |E| = fanout * |V| (random), so the actual
// edge count is slightly below fanout * |V|; always report the generated edge count.
#include <cstdint>
#include <string>

#include "graph/edge_list.hpp"

namespace tgl {

struct GenParams {
  std::string kind = "random";  // chain | layered | reconvergent | random
  NodeId nodes = 1000;
  std::uint64_t seed = 1;
  double fanout = 4.0;  // target average fanout (see above); chain ignores it
  NodeId layers = 0;    // layered/reconvergent: 0 => ~sqrt(nodes)
  NodeId span = 4;      // reconvergent: fanin may come from the previous `span` layers
  double delay_min = 1.0;
  double delay_max = 100.0;
  std::string delay_dist = "real";  // real: uniform double in [min, max); int: uniform integer in [min, max]
  bool shuffle = true;  // randomly relabel node ids and permute edge order
};

EdgeList generate(const GenParams& p);

class Rng {
 public:
  explicit Rng(std::uint64_t seed);
  std::uint64_t next();
  // Uniform integer in [lo, hi] (inclusive).
  std::uint64_t uniform(std::uint64_t lo, std::uint64_t hi);
  // Uniform double in [0, 1) with 53 random bits.
  double unit() { return static_cast<double>(next() >> 11) * 0x1.0p-53; }

 private:
  std::uint64_t s_[4];
};

}  // namespace tgl
