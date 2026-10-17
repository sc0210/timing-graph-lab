#pragma once
// Adjacency List (plan §4.2): one heap-allocated vector per node per direction.
// Easy to grow (add_edge is O(1) amortized) but neighbor storage is scattered.
#include <span>
#include <vector>

#include "graph/edge_list.hpp"

namespace tgl {

class AdjacencyList {
 public:
  explicit AdjacencyList(const EdgeList& el) : out_(el.num_nodes), in_(el.num_nodes), num_edges_(0) {
    for (const Edge& e : el.edges) add_edge(e.src, e.dst, e.delay);
  }

  NodeId num_nodes() const { return static_cast<NodeId>(out_.size()); }
  std::size_t num_edges() const { return num_edges_; }

  std::span<const Arc> fanout(NodeId v) const { return out_[v]; }
  std::span<const Arc> fanin(NodeId v) const { return in_[v]; }

  void add_edge(NodeId src, NodeId dst, Delay d) {
    out_[src].push_back({dst, d});
    in_[dst].push_back({src, d});
    ++num_edges_;
  }

  std::size_t memory_bytes() const {
    std::size_t bytes = sizeof(*this) + (out_.capacity() + in_.capacity()) * sizeof(std::vector<Arc>);
    for (const auto& a : out_) bytes += a.capacity() * sizeof(Arc);
    for (const auto& a : in_) bytes += a.capacity() * sizeof(Arc);
    return bytes;
  }

 private:
  std::vector<std::vector<Arc>> out_;
  std::vector<std::vector<Arc>> in_;
  std::size_t num_edges_;
};

}  // namespace tgl
