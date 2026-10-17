#include "sta/incremental.hpp"

#include <algorithm>
#include <stdexcept>

namespace tgl {

IncrementalTimer::IncrementalTimer(CsrGraph& g, TimingConstraints c)
    : g_(g),
      c_(std::move(c)),
      rank_(g.num_nodes()),
      mark_(g.num_nodes(), 0),
      in_queue_(g.num_nodes(), 0),
      changed_(g.num_nodes(), 0) {
  TopoResult topo = kahn_topo_sort(g_);
  if (!topo.is_dag) throw std::invalid_argument("IncrementalTimer requires a DAG");
  order_ = std::move(topo.order);
  for (std::uint32_t i = 0; i < order_.size(); ++i) rank_[order_[i]] = i;
  full_recompute();
}

void IncrementalTimer::full_recompute() {
  s_.resize(g_.num_nodes());
  run_full_sta(g_, order_, c_, s_);
}

void IncrementalTimer::reset(const EdgeList& el) {
  for (EdgeId e = 0; e < el.num_edges(); ++e) g_.set_edge_delay(e, el.edges[e].delay);
  seeds_fwd_.clear();
  seeds_bwd_.clear();
  pending_edges_ = 0;
  full_recompute();
}

void IncrementalTimer::set_edge_delay(EdgeId e, Delay d) {
  if (g_.edge_delay(e) == d) return;
  g_.set_edge_delay(e, d);
  seeds_fwd_.push_back(g_.edge_dst(e));  // AT(dst) may change
  seeds_bwd_.push_back(g_.edge_src(e));  // RT(src) may change
  ++pending_edges_;
}

// Every node reachable from seeds (through fanout if forward, else fanin), seeds included.
// Leaves mark_ set for the returned nodes; callers must clear it.
void IncrementalTimer::collect_cone(const std::vector<NodeId>& seeds, bool forward, std::vector<NodeId>& out) {
  out.clear();
  for (NodeId s : seeds)
    if (!mark_[s]) {
      mark_[s] = 1;
      out.push_back(s);
    }
  for (std::size_t i = 0; i < out.size(); ++i) {
    const NodeId v = out[i];
    for (const Arc& a : forward ? g_.fanout(v) : g_.fanin(v))
      if (!mark_[a.node]) {
        mark_[a.node] = 1;
        out.push_back(a.node);
      }
  }
}

AffectedRegion IncrementalTimer::affected_region() {
  AffectedRegion r;
  std::vector<NodeId> fwd, bwd;
  collect_cone(seeds_fwd_, true, fwd);
  r.fwd = fwd.size();
  // Count the union: nodes of the backward cone not already marked by the forward cone.
  for (NodeId v : fwd) changed_[v] = 1;
  for (NodeId v : fwd) mark_[v] = 0;
  collect_cone(seeds_bwd_, false, bwd);
  r.bwd = bwd.size();
  r.either = r.fwd;
  for (NodeId v : bwd) r.either += !changed_[v];
  for (NodeId v : bwd) mark_[v] = 0;
  for (NodeId v : fwd) changed_[v] = 0;
  return r;
}

void IncrementalTimer::mark_changed(NodeId v, IncrementalStats& st) {
  if (!changed_[v]) {
    changed_[v] = 1;
    changed_list_.push_back(v);
    ++st.changed_nodes;
  }
}

bool IncrementalTimer::set_at(NodeId v, Delay nv, IncrementalStats& st) {
  ++st.forward_evals;
  if (nv == s_.at[v]) return false;
  s_.at[v] = nv;
  s_.slack[v] = s_.rt[v] - nv;
  ++st.at_changed;
  mark_changed(v, st);
  return true;
}

bool IncrementalTimer::set_rt(NodeId v, Delay nv, IncrementalStats& st) {
  ++st.backward_evals;
  if (nv == s_.rt[v]) return false;
  s_.rt[v] = nv;
  s_.slack[v] = nv - s_.at[v];
  ++st.rt_changed;
  mark_changed(v, st);
  return true;
}

IncrementalStats IncrementalTimer::update(IncMode mode) {
  IncrementalStats st;
  st.updated_edges = pending_edges_;
  // AT and RT do not depend on each other, so the two passes are independent.
  if (mode == IncMode::Conservative) {
    std::vector<NodeId> region;
    auto by_rank = [this](NodeId a, NodeId b) { return rank_[a] < rank_[b]; };
    collect_cone(seeds_fwd_, true, region);
    st.affected_fwd = region.size();
    for (NodeId v : region) mark_[v] = 0;
    std::sort(region.begin(), region.end(), by_rank);
    for (NodeId v : region) set_at(v, eval_arrival(g_, s_.at, c_, v), st);

    collect_cone(seeds_bwd_, false, region);
    st.affected_bwd = region.size();
    for (NodeId v : region) mark_[v] = 0;
    std::sort(region.begin(), region.end(), [&](NodeId a, NodeId b) { return by_rank(b, a); });
    for (NodeId v : region) set_rt(v, eval_required(g_, s_.rt, c_, v), st);
  } else {
    std::priority_queue<std::uint32_t, std::vector<std::uint32_t>, std::greater<>> fwd;  // min rank first
    for (NodeId v : seeds_fwd_)
      if (!in_queue_[v]) in_queue_[v] = 1, fwd.push(rank_[v]);
    while (!fwd.empty()) {
      const NodeId v = order_[fwd.top()];
      fwd.pop();
      in_queue_[v] = 0;
      if (set_at(v, eval_arrival(g_, s_.at, c_, v), st))
        for (const Arc& a : g_.fanout(v))
          if (!in_queue_[a.node]) in_queue_[a.node] = 1, fwd.push(rank_[a.node]);
    }
    std::priority_queue<std::uint32_t> bwd;  // max rank first
    for (NodeId v : seeds_bwd_)
      if (!in_queue_[v]) in_queue_[v] = 1, bwd.push(rank_[v]);
    while (!bwd.empty()) {
      const NodeId v = order_[bwd.top()];
      bwd.pop();
      in_queue_[v] = 0;
      if (set_rt(v, eval_required(g_, s_.rt, c_, v), st))
        for (const Arc& a : g_.fanin(v))
          if (!in_queue_[a.node]) in_queue_[a.node] = 1, bwd.push(rank_[a.node]);
    }
  }
  for (NodeId v : changed_list_) changed_[v] = 0;
  changed_list_.clear();
  seeds_fwd_.clear();
  seeds_bwd_.clear();
  pending_edges_ = 0;
  return st;
}

}  // namespace tgl
