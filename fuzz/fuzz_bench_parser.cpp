// Fuzz target: the ISCAS .bench parser and netlist-to-timing-graph conversion on untrusted
// input. Any input must be rejected with std::runtime_error or produce a netlist whose
// timing graph passes validate(); a cyclic graph (combinational loop) must be detected.
#include <cstdint>
#include <stdexcept>
#include <string_view>

#include "graph/bench_netlist.hpp"
#include "graph/csr_graph.hpp"
#include "graph/traversal.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const std::string_view text(reinterpret_cast<const char*>(data), size);
  tgl::BenchNetlist n;
  try {
    n = tgl::parse_bench(text, "<fuzz>");
  } catch (const std::runtime_error&) {
    return 0;
  }
  const tgl::BenchTimingGraph b = tgl::to_timing_graph(n);
  if (b.names.size() != b.graph.num_nodes || !tgl::validate(b.graph)) __builtin_trap();
  const tgl::CsrGraph g(b.graph);
  const tgl::TopoResult t = tgl::kahn_topo_sort(g);
  if (t.is_dag != tgl::dfs_topo_sort(g).is_dag) __builtin_trap();
  return 0;
}
