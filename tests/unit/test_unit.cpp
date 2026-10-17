// L1 unit tests. T01-T14 refer to the required test cases of plan §7.2.
#include <cmath>
#include <cstdio>

#include "../common.hpp"
#include "../test_main.hpp"
#include "graph/adjacency_list.hpp"
#include "graph/generators.hpp"
#include "graph/graph_stats.hpp"
#include "sta/incremental.hpp"
#include "sta/level_pool.hpp"

using namespace tgl;
using namespace tgl::test;

TEST(T01_empty_graph) {
  const EdgeList el{0, {}};
  CHECK(validate(el));
  for (int pass = 0; pass < 2; ++pass) {
    CsrGraph c(el);
    AdjacencyList a(el);
    CHECK_EQ(c.num_nodes(), 0u);
    CHECK_EQ(c.num_edges(), 0u);
    CHECK_EQ(a.num_nodes(), 0u);
    CHECK(kahn_topo_sort(c).is_dag);
    CHECK(dfs_topo_sort(a).is_dag);
    const TimingState s = full_sta(c, {});
    CHECK(s.at.empty() && s.rt.empty() && s.slack.empty());
    CHECK_EQ(levelize(c, kahn_topo_sort(c).order).num_levels(), 0u);
  }
  CHECK(oracle::reference_engine(el, {}).has_value());
  CHECK_EQ(generate({.kind = "random", .nodes = 0}).num_edges(), 0u);
}

TEST(T02_single_node_uses_given_initial_values) {
  CsrGraph g(EdgeList{1, {}});
  const TimingState s = full_sta(g, {.default_arrival = 1.5, .default_required = 4.0});
  CHECK_EQ(s.at[0], 1.5);
  CHECK_EQ(s.rt[0], 4.0);
  CHECK_EQ(s.slack[0], 2.5);
}

TEST(T03_single_path_sums_delays) {
  CsrGraph g(EdgeList{4, {{0, 1, 1.25}, {1, 2, 2.5}, {2, 3, 4.0}}});
  const TimingState s = full_sta(g, {.default_arrival = 10, .default_required = 20});
  CHECK_EQ(s.at[3], 10 + 1.25 + 2.5 + 4.0);
  CHECK_EQ(s.rt[0], 20 - 4.0 - 2.5 - 1.25);
}

TEST(T04_reconvergence_takes_max_arrival_and_min_required) {
  // Plan §3.4: AT(E) = 8 via B->D->E, not 5 via A->C->E (BFS hop count is irrelevant).
  CsrGraph g(plan_example());
  const TimingState s = full_sta(g, {.default_required = 10});
  CHECK_EQ(s.at[4], 8.0);
  CHECK_EQ(s.at[2], 2.0);
  CHECK_EQ(s.at[3], 7.0);
  CHECK_EQ(s.rt[3], 9.0);
  CHECK_EQ(s.rt[1], 2.0);
  CHECK_EQ(s.rt[0], 5.0);
  CHECK_EQ(s.slack[1], 2.0);
  // Fork then reconverge: 0->1 (1), 0->2 (5), 1->3 (1), 2->3 (1). RT(0) = min(10-1-1, 10-1-5) = 4.
  CsrGraph d(EdgeList{4, {{0, 1, 1}, {0, 2, 5}, {1, 3, 1}, {2, 3, 1}}});
  const TimingState t = full_sta(d, {.default_required = 10});
  CHECK_EQ(t.at[3], 6.0);
  CHECK_EQ(t.rt[0], 4.0);
}

TEST(T05_multiple_sources_with_distinct_arrivals) {
  AdjacencyList g(plan_example());
  TimingConstraints c{.default_required = 20};
  c.arrival_override = {4, 0.5, 0, 0, 0};  // A=4, B=0.5 -> A->C->E = 9 beats 8.5
  const TimingState s = full_sta(g, c);
  CHECK_EQ(s.at[0], 4.0);
  CHECK_EQ(s.at[1], 0.5);
  CHECK_EQ(s.at[3], 7.5);
  CHECK_EQ(s.at[4], 9.0);
  std::vector<Delay> push;
  propagate_arrival_push(g, kahn_topo_sort(g).order, c, push);
  CHECK(push == s.at);
}

TEST(T06_multiple_sinks_propagate_required_backward) {
  // 0 -> 1 (2), 0 -> 2 (3); sinks 1 (r0 = 10) and 2 (r0 = 6): RT(0) = min(8, 3) = 3.
  CsrGraph g(EdgeList{3, {{0, 1, 2}, {0, 2, 3}}});
  TimingConstraints c;
  c.required_override = {0, 10, 6};
  const TimingState s = full_sta(g, c);
  CHECK_EQ(s.rt[1], 10.0);
  CHECK_EQ(s.rt[2], 6.0);
  CHECK_EQ(s.rt[0], 3.0);
}

TEST(T07_cycle_injection_is_detected) {
  EdgeList el = generate({.kind = "layered", .nodes = 500, .seed = 3, .layers = 10});
  CHECK(kahn_topo_sort(CsrGraph(el)).is_dag);
  // Close a cycle along an existing edge: dst -> src.
  el.edges.push_back({el.edges[0].dst, el.edges[0].src, 1});
  CsrGraph g(el);
  const TopoResult k = kahn_topo_sort(g);
  CHECK(!k.is_dag);
  CHECK(k.order.size() < el.num_nodes);
  CHECK(!dfs_topo_sort(g).is_dag);
  CHECK(!oracle::reference_engine(el, {}).has_value());
  CHECK(!oracle::relaxation_oracle(el, {}).has_value());
  bool threw = false;
  try {
    IncrementalTimer t(g, {});
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);
  CHECK(!validate(EdgeList{2, {{1, 1, 1}}}));  // self loops rejected by default
  CHECK(validate(EdgeList{2, {{1, 1, 1}}}, nullptr, /*allow_self_loops=*/true));
}

TEST(T08_zero_delay_edges_are_not_uninitialized) {
  // All delays zero and arrival zero: every AT must be exactly 0, not -inf.
  CsrGraph g(EdgeList{4, {{0, 1, 0}, {1, 2, 0}, {0, 2, 0}, {2, 3, 0}}});
  const TimingState s = full_sta(g, {.default_required = 0});
  for (NodeId v = 0; v < 4; ++v) {
    CHECK_EQ(s.at[v], 0.0);
    CHECK_EQ(s.rt[v], 0.0);
    CHECK(std::isfinite(s.slack[v]));
  }
  CHECK(validate(EdgeList{2, {{0, 1, 0}}}));
}

TEST(T09_single_edge_update_matches_full_recompute) {
  CsrGraph g(plan_example());
  const TimingConstraints c{.default_required = 10};
  IncrementalTimer t(g, c);
  t.set_edge_delay(0, 9);  // A->C 2 -> 9: A->C->E = 12 becomes critical
  const auto st = t.update();
  CHECK_EQ(t.state().at[4], 12.0);
  CHECK_EQ(t.state().slack[0], -2.0);
  CHECK_EQ(st.at_changed, 2u);  // C, E
  CHECK_EQ(st.rt_changed, 1u);  // A
  CHECK(bit_equal(t.state(), full_sta(g, c)));
}

TEST(T10_increase_and_decrease_both_match) {
  for (IncMode mode : {IncMode::Conservative, IncMode::EarlyStop}) {
    EdgeList el = plan_example();
    CsrGraph g(el);
    const TimingConstraints c{.default_required = 10};
    IncrementalTimer t(g, c);
    for (Delay d : {12.0, 0.5, 7.0, 0.0, 3.0}) {  // B->D up, down, back, zero
      t.set_edge_delay(2, d);
      t.update(mode);
      el.edges[2].delay = d;
      CHECK(matches(t.state(), *oracle::reference_engine(el, c)));
    }
  }
}

TEST(T10b_early_stop_halts_when_value_unchanged) {
  CsrGraph g(plan_example());
  IncrementalTimer t(g, {.default_required = 10});
  t.set_edge_delay(1, 4);  // C->E 3 -> 4: A->C->E = 6 still < 8, AT(E) unchanged
  const AffectedRegion region = t.affected_region();
  CHECK_EQ(region.fwd, 1u);  // {E}
  CHECK_EQ(region.bwd, 2u);  // {C, A}
  const auto st = t.update(IncMode::EarlyStop);
  CHECK_EQ(st.at_changed, 0u);
  CHECK_EQ(st.forward_evals, 1u);
  CHECK_EQ(st.rt_changed, 2u);  // RT(C) = 6, RT(A) = 4
}

TEST(T11_overlapping_multi_edge_updates) {
  // Updates on the same path, the same edge twice, and edges sharing a reconvergence point.
  for (IncMode mode : {IncMode::Conservative, IncMode::EarlyStop}) {
    EdgeList el = plan_example();
    CsrGraph g(el);
    const TimingConstraints c{.default_required = 10};
    IncrementalTimer t(g, c);
    const std::pair<EdgeId, Delay> batch[] = {{0, 6}, {1, 1}, {0, 4}, {3, 0}, {2, 2}};
    for (auto [e, d] : batch) {
      t.set_edge_delay(e, d);
      el.edges[e].delay = d;
    }
    const auto st = t.update(mode);
    CHECK(matches(t.state(), *oracle::reference_engine(el, c)));
    CHECK_EQ(st.updated_edges, 5u);
  }
}

TEST(T12_parallel_matches_single_thread) {
  const EdgeList el = generate({.kind = "layered", .nodes = 20000, .seed = 9, .layers = 20});
  CsrGraph g(el);
  const TimingConstraints c{.default_required = 5000};
  const TimingState serial = full_sta(g, c);
  const Levels L = levelize(g, kahn_topo_sort(g).order);
  for (BarrierKind kind : {BarrierKind::Spin, BarrierKind::Block})
  for (int threads : {1, 2, 3, 4, 8}) {
    LevelPool pool(threads, kind);
    for (int rep = 0; rep < 3; ++rep) {  // pool reuse across runs
      TimingState p;
      p.resize(g.num_nodes());
      propagate_arrival_parallel(g, L, c, p.at, pool);
      propagate_required_parallel(g, L, c, p.rt, pool);
      compute_slack(p);
      CHECK(matches(p, serial));
    }
  }
}

TEST(T13_list_and_csr_agree) {
  const EdgeList el = generate({.kind = "random", .nodes = 3000, .seed = 3, .fanout = 3});
  AdjacencyList a(el);
  CsrGraph c(el);
  CHECK_EQ(a.num_nodes(), c.num_nodes());
  CHECK_EQ(a.num_edges(), c.num_edges());
  bool same = true;
  for (NodeId v = 0; same && v < a.num_nodes(); ++v) {
    auto ao = a.fanout(v);
    auto co = c.fanout(v);
    auto ai = a.fanin(v);
    auto ci = c.fanin(v);
    same = ao.size() == co.size() && ai.size() == ci.size();
    for (std::size_t i = 0; same && i < ao.size(); ++i) same = ao[i].node == co[i].node && ao[i].delay == co[i].delay;
    for (std::size_t i = 0; same && i < ai.size(); ++i) same = ai[i].node == ci[i].node && ai[i].delay == ci[i].delay;
  }
  CHECK(same);
  CHECK(kahn_topo_sort(a).order == kahn_topo_sort(c).order);
  const TimingConstraints tc{.default_required = 900};
  CHECK(matches(full_sta(a, tc), full_sta(c, tc)));
}

TEST(T14_random_dag_topological_order_valid_and_consistent) {
  for (std::uint64_t seed = 1; seed <= 5; ++seed) {
    const EdgeList el = generate({.kind = "random", .nodes = 4000, .seed = seed});
    CsrGraph g(el);
    CHECK(is_topological_order(g, kahn_topo_sort(g).order));
    CHECK(is_topological_order(g, dfs_topo_sort(g).order));
    const TimingConstraints c{.default_required = 700};
    CHECK(matches(full_sta(g, c), *oracle::reference_engine(el, c)));
  }
}

TEST(bfs_order_is_not_always_topological) {
  // 0->1->2->3 and 0->3: BFS reaches 3 before 2 although 3 depends on 2.
  CsrGraph g(EdgeList{4, {{0, 3, 1}, {0, 1, 1}, {1, 2, 1}, {2, 3, 1}}});
  CHECK(!is_topological_order(g, bfs_order(g).order));
  CHECK(is_topological_order(g, kahn_topo_sort(g).order));
  const Levels L = levelize(g, kahn_topo_sort(g).order);
  CHECK_EQ(L.num_levels(), 4u);
  CHECK_EQ(L.level(3)[0], 3u);
}

TEST(csr_delay_update_touches_both_directions) {
  CsrGraph g(plan_example());
  g.set_edge_delay(3, 42);  // D->E
  CHECK_EQ(g.edge_delay(3), 42.0);
  CHECK_EQ(g.edge_src(3), 3u);
  CHECK_EQ(g.edge_dst(3), 4u);
  bool in_ok = false;
  for (const Arc& a : g.fanin(4)) in_ok |= a.node == 3 && a.delay == 42;
  CHECK(in_ok);
}

TEST(adjacency_list_add_edge) {
  AdjacencyList a(plan_example());
  a.add_edge(0, 4, 20);
  CHECK_EQ(a.num_edges(), 5u);
  CHECK_EQ(full_sta(a, {.default_required = 30}).at[4], 20.0);
}

TEST(compare_tolerance_rules) {
  const Tolerance t;
  CHECK(close(1.0, 1.0 + 1e-12, t));
  CHECK(!close(1.0, 1.0 + 1e-6, t));
  CHECK(close(1e12, 1e12 * (1 + 1e-10), t));  // relative term
  CHECK(!close(std::nan(""), std::nan(""), t));
  CHECK(close(kPosInf, kPosInf, t));
  const auto r = compare_values({1.0, kNegInf}, {1.0, kNegInf});
  CHECK(!r.pass);  // infinite results are never acceptable
  CHECK_EQ(r.non_finite, 1u);
}

TEST(edge_list_save_load_round_trip) {
  const EdgeList el = generate({.kind = "reconvergent", .nodes = 800, .seed = 77, .layers = 10});
  char path[] = "/tmp/tgl_roundtrip_XXXXXX";
  const int fd = mkstemp(path);
  CHECK(fd >= 0);
  close(fd);
  write_edge_list(el, path);
  const EdgeList back = read_edge_list(path);
  std::remove(path);
  CHECK_EQ(fingerprint(back), fingerprint(el));
}

TEST(graph_stats_counts) {
  const GraphStats s = compute_stats(plan_example());
  CHECK_EQ(s.sources, 2u);
  CHECK_EQ(s.sinks, 1u);
  CHECK_EQ(s.max_fanin, 2u);
  CHECK_EQ(s.max_fanout, 1u);
  CHECK_EQ(s.fanout_hist[0] + s.fanout_hist[1], 5u);
}
