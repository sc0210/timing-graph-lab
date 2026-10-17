#pragma once
// Timing constraints and per-node timing results (plan §3).
//   AT(v)    = a0(v)                              if pred(v) = {}
//            = max_{u in pred(v)} AT(u) + w(u,v)  otherwise
//   RT(v)    = r0(v)                              if succ(v) = {}
//            = min_{u in succ(v)} RT(u) - w(v,u)  otherwise
//   Slack(v) = RT(v) - AT(v)
#include <algorithm>
#include <cstdint>
#include <vector>

#include "graph/edge_list.hpp"

namespace tgl {

struct TimingConstraints {
  Delay default_arrival = 0;   // a0 for every source without an override
  Delay default_required = 0;  // r0 for every sink without an override (e.g. clock period)
  std::vector<Delay> arrival_override;   // empty, or size n (value used only for sources)
  std::vector<Delay> required_override;  // empty, or size n (value used only for sinks)

  Delay a0(NodeId v) const { return arrival_override.empty() ? default_arrival : arrival_override[v]; }
  Delay r0(NodeId v) const { return required_override.empty() ? default_required : required_override[v]; }
};

struct TimingState {
  std::vector<Delay> at, rt, slack;

  void resize(NodeId n) {
    at.assign(n, kNegInf);
    rt.assign(n, kPosInf);
    slack.assign(n, 0);
  }
};

struct TimingSummary {
  Delay wns = kPosInf;  // worst (minimum) slack over all nodes
  Delay tns = 0;        // sum of negative slack over sinks
  Delay max_arrival = kNegInf;
  std::uint64_t checksum = 0;  // order-sensitive hash of (AT, RT) bit patterns
};

template <class G>
TimingSummary summarize(const G& g, const TimingState& s) {
  TimingSummary sum;
  std::uint64_t h = 1469598103934665603ULL;
  auto mix = [&h](Delay d) {
    std::uint64_t bits;
    static_assert(sizeof bits == sizeof d);
    __builtin_memcpy(&bits, &d, sizeof d);
    // splitmix64 finalizer spreads high mantissa/exponent bits before FNV-style folding.
    bits ^= bits >> 30;
    bits *= 0xBF58476D1CE4E5B9ULL;
    bits ^= bits >> 27;
    bits *= 0x94D049BB133111EBULL;
    bits ^= bits >> 31;
    h = (h ^ bits) * 1099511628211ULL;
  };
  for (NodeId v = 0; v < g.num_nodes(); ++v) {
    sum.wns = std::min(sum.wns, s.slack[v]);
    sum.max_arrival = std::max(sum.max_arrival, s.at[v]);
    if (g.fanout(v).empty() && s.slack[v] < 0) sum.tns += s.slack[v];
    mix(s.at[v]);
    mix(s.rt[v]);
  }
  sum.checksum = h;
  return sum;
}

}  // namespace tgl
