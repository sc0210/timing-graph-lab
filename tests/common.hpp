#pragma once
// Helpers shared by the test suites.
#include "graph/csr_graph.hpp"
#include "graph/traversal.hpp"
#include "sta/compare.hpp"
#include "sta/oracle.hpp"
#include "sta/required.hpp"

namespace tgl::test {

inline const char* kKinds[] = {"chain", "layered", "reconvergent", "random"};

// Plan §3.4 example: A=0 B=1 C=2 D=3 E=4; A->C 2, C->E 3, B->D 7, D->E 1.
inline EdgeList plan_example() { return {5, {{0, 2, 2}, {2, 4, 3}, {1, 3, 7}, {3, 4, 1}}}; }

template <class G>
TimingState full_sta(const G& g, const TimingConstraints& c) {
  TimingState s;
  s.resize(g.num_nodes());
  run_full_sta(g, kahn_topo_sort(g).order, c, s);
  return s;
}

inline bool matches(const TimingState& got, const TimingState& ref) { return compare_timing(got, ref).pass; }
inline bool bit_equal(const TimingState& a, const TimingState& b) {
  return a.at == b.at && a.rt == b.rt && a.slack == b.slack;
}

}  // namespace tgl::test
