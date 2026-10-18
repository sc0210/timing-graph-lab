// Differential fuzz target: decodes the input into a small DAG (up to 48 nodes, forward
// edges only, so always acyclic), per-node constraints and a sequence of delay edits, then
//   1. checks every engine variant against the reference engine (tests/engine_check.hpp),
//   2. replays the edits through the incremental timer, alternating modes, and checks the
//      result against the reference after every update.
// A mismatch aborts with a description, so libFuzzer saves the input as a crash.
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "../tests/engine_check.hpp"
#include "byte_reader.hpp"

namespace {

// Delays with special cases: zero, tiny, huge, ties, and arbitrary fractions.
tgl::Delay pick_delay(tgl::fuzz::ByteReader& in) {
  switch (in.byte() % 8) {
    case 0:
      return 0;
    case 1:
      return 1;
    case 2:
      return 1e-3 * in.range(0, 1000);
    case 3:
      return 1e5 * in.range(0, 1000);
    default:
      return in.range(0, 100000) / 64.0;
  }
}

[[noreturn]] void report(const char* what, const std::string& detail) {
  std::fprintf(stderr, "fuzz_engine: %s: %s\n", what, detail.c_str());
  std::abort();
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  using namespace tgl;
  static test::EnginePools pools;
  fuzz::ByteReader in(data, size);

  EdgeList el;
  el.num_nodes = in.range(0, 48);
  const std::uint32_t m = el.num_nodes < 2 ? 0 : in.range(0, 4 * el.num_nodes);
  for (std::uint32_t k = 0; k < m; ++k) {
    const NodeId u = in.range(0, el.num_nodes - 2);
    const NodeId v = in.range(u + 1, el.num_nodes - 1);
    el.edges.push_back({u, v, pick_delay(in)});
  }
  TimingConstraints c;
  c.default_arrival = in.range(0, 20) - 10.0;
  c.default_required = in.range(0, 2000) / 4.0;
  if (in.flag()) {
    c.arrival_override.resize(el.num_nodes);
    for (auto& a : c.arrival_override) a = in.range(0, 400) / 8.0 - 10;
  }
  if (in.flag()) {
    c.required_override.resize(el.num_nodes);
    for (auto& r : c.required_override) r = in.range(0, 4000) / 8.0;
  }

  if (auto err = test::check_all_engines(el, c, pools); !err.empty()) report("engine", err);

  if (el.edges.empty()) return 0;
  CsrGraph g(el);
  IncrementalTimer timer(g, c);
  const std::uint32_t rounds = in.range(0, 12);
  for (std::uint32_t round = 0; round < rounds; ++round) {
    const std::uint32_t batch = in.range(1, 6);
    for (std::uint32_t b = 0; b < batch; ++b) {
      const EdgeId e = in.range(0, static_cast<std::uint32_t>(el.num_edges() - 1));
      const Delay d = pick_delay(in);
      el.edges[e].delay = d;
      timer.set_edge_delay(e, d);
    }
    timer.update(in.flag() ? IncMode::Conservative : IncMode::EarlyStop);
    const CompareResult r = compare_timing(timer.state(), *oracle::reference_engine(el, c));
    if (!r.pass) report("incremental", std::to_string(r.mismatches) + " mismatching value(s)");
  }
  return 0;
}
