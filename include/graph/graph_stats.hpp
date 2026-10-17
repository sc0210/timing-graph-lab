#pragma once
// Graph shape statistics (plan §5.3 item 7, Experiment B): source/sink counts and
// fanout/fanin distributions. Computed from the EdgeList only.
#include <array>
#include <cstdint>
#include <string>

#include "graph/edge_list.hpp"

namespace tgl {

struct GraphStats {
  // Histogram buckets by degree: 0, 1, 2, 3-4, 5-8, 9-16, 17-32, 33+.
  static constexpr std::array<const char*, 8> kBuckets = {"0", "1", "2", "3-4", "5-8", "9-16", "17-32", "33+"};

  NodeId nodes = 0;
  std::size_t edges = 0;
  NodeId sources = 0;   // fanin == 0
  NodeId sinks = 0;     // fanout == 0
  NodeId isolated = 0;  // fanin == fanout == 0
  std::uint32_t max_fanout = 0, max_fanin = 0;
  double avg_fanout = 0;  // edges / nodes
  std::array<std::uint64_t, 8> fanout_hist{}, fanin_hist{};

  std::string to_json() const;
};

GraphStats compute_stats(const EdgeList& el);

}  // namespace tgl
