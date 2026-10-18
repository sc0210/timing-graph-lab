// Fuzz target: the tgl-edgelist parser on untrusted input (SECURITY.md). Any input must
// either be rejected with std::runtime_error or produce an EdgeList; it must never crash,
// leak, hang, or allocate memory unrelated to the input size. Accepted graphs are also
// validated and, when small, built and topologically sorted.
#include <cstdint>
#include <stdexcept>
#include <string_view>

#include "graph/csr_graph.hpp"
#include "graph/traversal.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const std::string_view text(reinterpret_cast<const char*>(data), size);
  tgl::EdgeList el;
  try {
    el = tgl::parse_edge_list(text, "<fuzz>", /*max_nodes=*/1u << 16);
  } catch (const std::runtime_error&) {
    return 0;
  }
  if (!tgl::validate(el)) return 0;
  const tgl::CsrGraph g(el);
  const tgl::TopoResult t = tgl::kahn_topo_sort(g);
  if (t.is_dag && !tgl::is_topological_order(g, t.order)) __builtin_trap();
  if (!t.is_dag && t.order.size() >= el.num_nodes) __builtin_trap();
  return 0;
}
