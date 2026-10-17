#include "graph/csr_graph.hpp"

namespace tgl {

CsrGraph::CsrGraph(const EdgeList& el)
    : out_offsets_(el.num_nodes + std::size_t{1}, 0),
      in_offsets_(el.num_nodes + std::size_t{1}, 0),
      out_dst_(el.num_edges()),
      in_src_(el.num_edges()),
      out_delay_(el.num_edges()),
      in_delay_(el.num_edges()),
      out_pos_(el.num_edges()),
      in_pos_(el.num_edges()) {
  // Counting sort keeps edge-list order within each node (stable).
  for (const Edge& e : el.edges) {
    ++out_offsets_[e.src + std::size_t{1}];
    ++in_offsets_[e.dst + std::size_t{1}];
  }
  for (std::size_t v = 0; v < el.num_nodes; ++v) {
    out_offsets_[v + 1] += out_offsets_[v];
    in_offsets_[v + 1] += in_offsets_[v];
  }
  std::vector<std::uint64_t> out_cur(out_offsets_.begin(), out_offsets_.end() - 1);
  std::vector<std::uint64_t> in_cur(in_offsets_.begin(), in_offsets_.end() - 1);
  for (std::size_t id = 0; id < el.num_edges(); ++id) {
    const Edge& e = el.edges[id];
    const auto op = out_cur[e.src]++;
    const auto ip = in_cur[e.dst]++;
    out_dst_[op] = e.dst;
    out_delay_[op] = e.delay;
    in_src_[ip] = e.src;
    in_delay_[ip] = e.delay;
    out_pos_[id] = op;
    in_pos_[id] = ip;
  }
}

std::size_t CsrGraph::memory_bytes() const {
  return sizeof(*this) + (out_offsets_.capacity() + in_offsets_.capacity()) * sizeof(std::uint64_t) +
         (out_dst_.capacity() + in_src_.capacity()) * sizeof(NodeId) +
         (out_delay_.capacity() + in_delay_.capacity()) * sizeof(Delay);
}

}  // namespace tgl
