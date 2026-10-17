#pragma once
// Compressed Sparse Row graph (plan §4.3), structure-of-arrays layout:
//   out_offsets / out_dst / out_delay  - fanout of u is [out_offsets[u], out_offsets[u+1])
//   in_offsets  / in_src  / in_delay   - fanin, kept so Required and Incremental need no extra build
// Arcs keep edge-list order within each node, so iteration order matches AdjacencyList.
// Delays can be updated in place; structural edits require a rebuild.
#include <cstdint>
#include <vector>

#include "graph/edge_list.hpp"

namespace tgl {

// Random-access view over parallel node/delay arrays; yields Arc by value.
class ArcView {
 public:
  class iterator {
   public:
    iterator(const NodeId* n, const Delay* d) : n_(n), d_(d) {}
    Arc operator*() const { return {*n_, *d_}; }
    iterator& operator++() {
      ++n_;
      ++d_;
      return *this;
    }
    bool operator==(const iterator& o) const { return n_ == o.n_; }

   private:
    const NodeId* n_;
    const Delay* d_;
  };

  ArcView(const NodeId* n, const Delay* d, std::size_t size) : n_(n), d_(d), size_(size) {}
  std::size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }
  Arc operator[](std::size_t i) const { return {n_[i], d_[i]}; }
  iterator begin() const { return {n_, d_}; }
  iterator end() const { return {n_ + size_, d_ + size_}; }

 private:
  const NodeId* n_;
  const Delay* d_;
  std::size_t size_;
};

class CsrGraph {
 public:
  explicit CsrGraph(const EdgeList& el);

  NodeId num_nodes() const { return static_cast<NodeId>(out_offsets_.size() - 1); }
  std::size_t num_edges() const { return out_dst_.size(); }

  ArcView fanout(NodeId v) const {
    const auto b = out_offsets_[v];
    return {out_dst_.data() + b, out_delay_.data() + b, out_offsets_[v + 1] - b};
  }
  ArcView fanin(NodeId v) const {
    const auto b = in_offsets_[v];
    return {in_src_.data() + b, in_delay_.data() + b, in_offsets_[v + 1] - b};
  }

  NodeId edge_src(EdgeId e) const { return in_src_[in_pos_[e]]; }
  NodeId edge_dst(EdgeId e) const { return out_dst_[out_pos_[e]]; }
  Delay edge_delay(EdgeId e) const { return out_delay_[out_pos_[e]]; }
  void set_edge_delay(EdgeId e, Delay d) {
    out_delay_[out_pos_[e]] = d;
    in_delay_[in_pos_[e]] = d;
  }

  // Graph structure only (offsets + arcs), comparable with AdjacencyList::memory_bytes().
  std::size_t memory_bytes() const;
  // Extra edge-id -> position maps that exist only to support in-place delay updates.
  std::size_t update_index_bytes() const { return (out_pos_.capacity() + in_pos_.capacity()) * sizeof(std::uint64_t); }

 private:
  std::vector<std::uint64_t> out_offsets_, in_offsets_;
  std::vector<NodeId> out_dst_, in_src_;
  std::vector<Delay> out_delay_, in_delay_;
  std::vector<std::uint64_t> out_pos_, in_pos_;  // indexed by EdgeId
};

}  // namespace tgl
