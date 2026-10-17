// L2 property tests over many seeds and every generator: invariants that must hold
// for any DAG, independent of a reference implementation (plan §5.3, §7.1).
#include <cmath>
#include <set>

#include "../common.hpp"
#include "../test_main.hpp"
#include "graph/adjacency_list.hpp"
#include "graph/generators.hpp"
#include "graph/graph_stats.hpp"

using namespace tgl;
using namespace tgl::test;

namespace {
template <class F>
void for_graphs(F f) {
  for (const char* kind : kKinds)
    for (std::uint64_t seed = 1; seed <= 25; ++seed) {
      GenParams p{.kind = kind, .nodes = static_cast<NodeId>(50 + seed * 37), .seed = seed,
                  .fanout = 1.0 + double(seed % 5) * 0.7, .layers = static_cast<NodeId>(2 + seed % 9),
                  .delay_dist = seed % 2 ? "real" : "int"};
      f(generate(p), p);
    }
}
}  // namespace

TEST(generators_satisfy_structural_requirements) {  // plan §5.3 items 1-5
  for_graphs([](const EdgeList& el, const GenParams& p) {
    CHECK_EQ(el.num_nodes, p.nodes);
    std::string err;
    CHECK(validate(el, &err));  // ids in range, no self loops, finite non-negative delays
    for (const Edge& e : el.edges) CHECK(e.delay >= p.delay_min && e.delay <= p.delay_max);
    CHECK(kahn_topo_sort(CsrGraph(el)).is_dag);
    std::set<std::pair<NodeId, NodeId>> uniq;
    for (const Edge& e : el.edges) uniq.insert({e.src, e.dst});
    CHECK_EQ(uniq.size(), el.num_edges());  // no parallel duplicates
  });
}

TEST(generators_are_deterministic_per_seed) {  // plan §5.3 item 6
  for (const char* kind : kKinds) {
    GenParams p{.kind = kind, .nodes = 5000, .seed = 5, .layers = 50};
    CHECK_EQ(fingerprint(generate(p)), fingerprint(generate(p)));
    GenParams q = p;
    q.seed = 6;
    if (std::string(kind) != "chain" || q.delay_max > q.delay_min) CHECK(fingerprint(generate(p)) != fingerprint(generate(q)));
  }
}

TEST(generator_fanout_is_near_target_and_not_truncated) {  // plan §5.2 note, §5.3 item 8
  for (const char* kind : {"layered", "reconvergent", "random"}) {
    const EdgeList el = generate({.kind = kind, .nodes = 100000, .seed = 20261017, .fanout = 4, .layers = 100});
    const GraphStats s = compute_stats(el);
    CHECK(s.avg_fanout > 3.8 && s.avg_fanout <= 4.0);
    // High-fanout nodes must exist: with ~Poisson(4) fanout, some nodes exceed 12.
    CHECK(s.max_fanout >= 12);
    CHECK(s.sources > 0 && s.sinks > 0);
  }
}

TEST(both_topological_sorts_are_valid) {
  for_graphs([](const EdgeList& el, const GenParams&) {
    CsrGraph g(el);
    auto k = kahn_topo_sort(g);
    auto d = dfs_topo_sort(g);
    CHECK(k.is_dag && d.is_dag);
    CHECK(is_topological_order(g, k.order));
    CHECK(is_topological_order(g, d.order));
    CHECK_EQ(k.work, el.num_edges());
  });
}

TEST(arrival_dominates_every_edge_and_is_tight) {
  for_graphs([](const EdgeList& el, const GenParams&) {
    CsrGraph g(el);
    const TimingConstraints c{.default_required = 1000};
    const TimingState s = full_sta(g, c);
    std::vector<std::uint8_t> tight_at(el.num_nodes, 0), tight_rt(el.num_nodes, 0);
    for (const Edge& e : el.edges) {
      CHECK(s.at[e.dst] >= s.at[e.src] + e.delay);
      CHECK(s.rt[e.src] <= s.rt[e.dst] - e.delay);
      if (s.at[e.dst] == s.at[e.src] + e.delay) tight_at[e.dst] = 1;
      if (s.rt[e.src] == s.rt[e.dst] - e.delay) tight_rt[e.src] = 1;
    }
    for (NodeId v = 0; v < el.num_nodes; ++v) {
      CHECK(std::isfinite(s.at[v]) && std::isfinite(s.rt[v]));
      if (g.fanin(v).empty()) CHECK_EQ(s.at[v], 0.0); else CHECK(tight_at[v]);  // some pred attains the max
      if (g.fanout(v).empty()) CHECK_EQ(s.rt[v], 1000.0); else CHECK(tight_rt[v]);
      CHECK_EQ(s.slack[v], s.rt[v] - s.at[v]);
    }
  });
}

TEST(result_independent_of_order_and_direction) {
  for_graphs([](const EdgeList& el, const GenParams&) {
    CsrGraph g(el);
    const TimingConstraints c{.default_required = 500};
    TimingState a, b;
    run_full_sta(g, kahn_topo_sort(g).order, c, a);
    run_full_sta(g, dfs_topo_sort(g).order, c, b);
    CHECK(bit_equal(a, b));
    std::vector<Delay> push;
    propagate_arrival_push(g, kahn_topo_sort(g).order, c, push);
    CHECK(push == a.at);
  });
}

TEST(worst_slack_is_zero_when_required_equals_max_arrival) {
  for_graphs([](const EdgeList& el, const GenParams&) {
    if (el.num_nodes == 0) return;
    CsrGraph g(el);
    auto order = kahn_topo_sort(g).order;
    TimingConstraints c;
    std::vector<Delay> at;
    propagate_arrival(g, order, c, at);
    c.default_required = *std::max_element(at.begin(), at.end());
    TimingState s;
    run_full_sta(g, order, c, s);
    // Not exactly 0 with real delays: RT is summed backward and AT forward, so rounding differs (plan §7.4).
    CHECK(close(summarize(g, s).wns + c.default_required, c.default_required, Tolerance{}));
  });
}

TEST(delay_change_moves_arrival_monotonically) {
  for_graphs([](EdgeList el, const GenParams&) {
    if (el.edges.empty()) return;
    const TimingConstraints c;
    const TimingState before = full_sta(CsrGraph(el), c);
    el.edges[el.edges.size() / 2].delay += 50;
    const TimingState up = full_sta(CsrGraph(el), c);
    el.edges[el.edges.size() / 2].delay = 0;
    const TimingState down = full_sta(CsrGraph(el), c);
    for (NodeId v = 0; v < el.num_nodes; ++v) {
      CHECK(up.at[v] >= before.at[v]);
      CHECK(down.at[v] <= before.at[v]);
    }
  });
}
