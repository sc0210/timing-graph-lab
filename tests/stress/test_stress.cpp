// Stress tests: degenerate and extreme graphs that the four topology generators never
// produce. Each graph goes through every engine variant (tests/engine_check.hpp) and is
// compared with the reference engine; incremental cases also check that work stays local.
#include <cmath>
#include <numeric>

#include "../common.hpp"
#include "../engine_check.hpp"
#include "../test_main.hpp"
#include "graph/generators.hpp"

using namespace tgl;
using namespace tgl::test;

namespace {

EnginePools& pools() {
  static EnginePools p;
  return p;
}

void expect_all_engines(const EdgeList& el, const TimingConstraints& c, CheckOptions opt = {}) {
  CHECK(validate(el));
  const std::string err = check_all_engines(el, c, pools(), opt);
  if (!err.empty()) std::fprintf(stderr, "  engine mismatch: %s\n", err.c_str());
  CHECK(err.empty());
}

EdgeList star_fanout(NodeId leaves, Rng& rng) {
  EdgeList el{leaves + 1, {}};
  for (NodeId v = 1; v <= leaves; ++v) el.edges.push_back({0, v, 1 + 99 * rng.unit()});
  return el;
}

EdgeList star_fanin(NodeId leaves, Rng& rng) {
  EdgeList el{leaves + 1, {}};
  for (NodeId v = 1; v <= leaves; ++v) el.edges.push_back({v, 0, 1 + 99 * rng.unit()});
  return el;
}

}  // namespace

TEST(huge_fanout_star) {
  Rng rng(1);
  const EdgeList el = star_fanout(200000, rng);
  const TimingConstraints c{.default_required = 50};
  expect_all_engines(el, c);
  // RT of the hub is the minimum over 200,000 sinks: r0 - max delay.
  Delay max_d = 0;
  for (const Edge& e : el.edges) max_d = std::max(max_d, e.delay);
  CHECK_EQ(oracle::reference_engine(el, c)->rt[0], 50 - max_d);
}

TEST(huge_fanin_star) {
  Rng rng(2);
  const EdgeList el = star_fanin(200000, rng);
  expect_all_engines(el, {.default_required = 0});
  // Editing one fanin edge re-evaluates exactly one node forward (the hub).
  CsrGraph g(el);
  IncrementalTimer t(g, {.default_required = 0});
  t.set_edge_delay(7, 0.5);
  const auto st = t.update(IncMode::EarlyStop);
  CHECK_EQ(st.forward_evals, 1u);
  CHECK(matches(t.state(), *oracle::reference_engine(
                               [&] {
                                 EdgeList m = el;
                                 m.edges[7].delay = 0.5;
                                 return m;
                               }(),
                               {.default_required = 0})));
}

TEST(very_deep_chain) {
  // One million levels: recursion anywhere would overflow the stack; level-parallel
  // engines are skipped (one barrier per level makes them minutes long, see Experiment D).
  const NodeId n = 1000000;
  const EdgeList el = generate({.kind = "chain", .nodes = n, .seed = 3, .shuffle = true});
  expect_all_engines(el, {.default_required = 0}, {.parallel = false});
  // A change at the head of the chain must propagate through all n nodes.
  CsrGraph g(el);
  IncrementalTimer t(g, {});
  EdgeId head = 0;
  for (EdgeId e = 0; e < el.num_edges(); ++e)
    if (g.fanin(g.edge_src(e)).empty()) head = e;
  t.set_edge_delay(head, g.edge_delay(head) + 1);
  CHECK_EQ(t.update(IncMode::EarlyStop).at_changed, std::uint64_t{n - 1});
}

TEST(isolated_nodes_and_empty_edge_set) {
  TimingConstraints c{.default_arrival = 3, .default_required = 7};
  expect_all_engines(EdgeList{100000, {}}, c);
  const auto s = oracle::reference_engine(EdgeList{5, {}}, c);
  CHECK(s && s->slack[4] == 4.0);
}

TEST(parallel_duplicate_edges_take_extremes) {
  // Three parallel arcs 0->1 with delays 1, 5, 3: AT(1) uses 5, RT(0) uses 5.
  const EdgeList el{3, {{0, 1, 1}, {0, 1, 5}, {0, 1, 3}, {1, 2, 2}}};
  expect_all_engines(el, {.default_required = 20});
  const auto s = oracle::reference_engine(el, {.default_required = 20});
  CHECK_EQ(s->at[1], 5.0);
  CHECK_EQ(s->rt[0], 13.0);
  // Lowering the dominant duplicate must fall back to the next largest (3).
  CsrGraph g(el);
  IncrementalTimer t(g, {.default_required = 20});
  t.set_edge_delay(1, 0);
  t.update(IncMode::EarlyStop);
  CHECK_EQ(t.state().at[1], 3.0);
  CHECK_EQ(t.state().rt[0], 15.0);
}

TEST(wide_dynamic_range_of_delays) {
  // Delays log-uniform in [1e-3, 1e6]: sums lose low-order bits, so the comparison
  // relies on the §7.4 relative tolerance, not on exact decimal values.
  EdgeList el = generate({.kind = "random", .nodes = 20000, .seed = 4, .fanout = 3});
  Rng rng(5);
  for (Edge& e : el.edges) e.delay = std::pow(10.0, -3 + 9 * rng.unit());
  expect_all_engines(el, {.default_required = 1e7});
}

TEST(all_zero_delays) {
  EdgeList el = generate({.kind = "reconvergent", .nodes = 20000, .seed = 6, .layers = 50});
  for (Edge& e : el.edges) e.delay = 0;
  expect_all_engines(el, {});
  const auto s = oracle::reference_engine(el, {});
  CHECK(std::all_of(s->at.begin(), s->at.end(), [](Delay a) { return a == 0; }));
}

TEST(complete_dag) {
  // Every pair i < j connected: 79,800 edges on 400 nodes, depth 400, fanin up to 399.
  EdgeList el{400, {}};
  Rng rng(7);
  for (NodeId i = 0; i < 400; ++i)
    for (NodeId j = i + 1; j < 400; ++j) el.edges.push_back({i, j, 1 + 9 * rng.unit()});
  expect_all_engines(el, {.default_required = 1000});
}

TEST(complete_bipartite) {
  // K(300, 300): a single level of width 300 feeding another, 90,000 edges.
  EdgeList el{600, {}};
  Rng rng(8);
  for (NodeId i = 0; i < 300; ++i)
    for (NodeId j = 300; j < 600; ++j) el.edges.push_back({i, j, 1 + 9 * rng.unit()});
  expect_all_engines(el, {.default_required = 100});
}

TEST(per_node_constraints_everywhere) {
  // Random a0 for every source and r0 for every sink, including negative values.
  const EdgeList el = generate({.kind = "random", .nodes = 50000, .seed = 9, .fanout = 2});
  Rng rng(10);
  TimingConstraints c;
  c.arrival_override.resize(el.num_nodes);
  c.required_override.resize(el.num_nodes);
  for (auto& a : c.arrival_override) a = -50 + 100 * rng.unit();
  for (auto& r : c.required_override) r = -100 + 1000 * rng.unit();
  expect_all_engines(el, c);
}

TEST(incremental_storm_on_dense_graph) {
  // Thousands of overlapping edits on a dense DAG, both modes, checked after every batch.
  EdgeList el{300, {}};
  Rng rng(11);
  for (NodeId i = 0; i < 300; ++i)
    for (NodeId j = i + 1; j < 300; ++j)
      if (rng.unit() < 0.2) el.edges.push_back({i, j, 1 + 9 * rng.unit()});
  for (IncMode mode : {IncMode::Conservative, IncMode::EarlyStop}) {
    EdgeList cur = el;
    CsrGraph g(cur);
    const TimingConstraints c{.default_required = 200};
    IncrementalTimer t(g, c);
    for (int batch = 0; batch < 50; ++batch) {
      for (int k = 0; k < 60; ++k) {
        const auto e = static_cast<EdgeId>(rng.uniform(0, cur.num_edges() - 1));
        const Delay d = rng.unit() < 0.1 ? 0.0 : 20 * rng.unit();
        cur.edges[e].delay = d;
        t.set_edge_delay(e, d);
      }
      t.update(mode);
      CHECK(matches(t.state(), *oracle::reference_engine(cur, c)));
    }
  }
}
