// L3 differential tests: every engine variant against the independent oracles (plan §7.3).
#include "../common.hpp"
#include "../test_main.hpp"
#include "graph/adjacency_list.hpp"
#include "graph/generators.hpp"
#include "sta/incremental.hpp"
#include "sta/level_pool.hpp"

using namespace tgl;
using namespace tgl::test;

namespace {
// Random per-node a0 / r0 (only source / sink entries are used).
TimingConstraints random_constraints(NodeId n, Rng& rng, int variant) {
  TimingConstraints c{.default_arrival = 0, .default_required = 300};
  if (variant & 1) {
    c.arrival_override.resize(n);
    for (auto& a : c.arrival_override) a = 50 * rng.unit();
  }
  if (variant & 2) {
    c.required_override.resize(n);
    for (auto& r : c.required_override) r = 200 + 200 * rng.unit();
  }
  return c;
}
}  // namespace

TEST(oracles_agree_exhaustively_on_tiny_graphs) {
  // Every generator x 40 seeds x 4 source/sink constraint variants: path enumeration,
  // relaxation and the reference engine must coincide (validates the large-graph oracle).
  int graphs = 0;
  for (const char* kind : kKinds)
    for (std::uint64_t seed = 1; seed <= 40; ++seed) {
      const EdgeList el = generate({.kind = kind, .nodes = static_cast<NodeId>(6 + seed % 12), .seed = seed,
                                    .fanout = 1.0 + double(seed % 4) * 0.6, .layers = static_cast<NodeId>(3 + seed % 3)});
      Rng rng(seed);
      for (int variant = 0; variant < 4; ++variant) {
        const TimingConstraints c = random_constraints(el.num_nodes, rng, variant);
        const TimingState paths = oracle::path_oracle(el, c);
        const auto rel = oracle::relaxation_oracle(el, c);
        const auto ref = oracle::reference_engine(el, c);
        CHECK(rel && ref);
        CHECK(bit_equal(*rel, paths));
        CHECK(bit_equal(*ref, paths));
        ++graphs;
      }
    }
  CHECK(graphs == 4 * 40 * 4);
}

TEST(reference_engine_matches_relaxation_on_medium_graphs) {
  for (const char* kind : kKinds)
    for (std::uint64_t seed = 1; seed <= 4; ++seed) {
      const EdgeList el = generate({.kind = kind, .nodes = 3000, .seed = seed, .fanout = 3, .layers = 30});
      const TimingConstraints c{.default_required = 2000};
      CHECK(matches(*oracle::reference_engine(el, c), *oracle::relaxation_oracle(el, c)));
    }
}

TEST(all_engines_match_reference) {
  // List / CSR, pull / push / DFS order, level-parallel with 1-4 threads.
  for (const char* kind : kKinds)
    for (std::uint64_t seed = 1; seed <= 6; ++seed) {
      // Chains get one barrier per node, so keep them small enough for sanitizer builds.
      const NodeId n = std::string(kind) == "chain" ? 2000 : 20000;
      const EdgeList el = generate({.kind = kind, .nodes = n, .seed = seed, .fanout = 2.5, .layers = 50});
      Rng rng(seed);
      const TimingConstraints c = random_constraints(el.num_nodes, rng, int(seed % 4));
      const TimingState ref = *oracle::reference_engine(el, c);

      AdjacencyList a(el);
      CsrGraph g(el);
      CHECK(matches(full_sta(a, c), ref));
      CHECK(matches(full_sta(g, c), ref));
      TimingState d;
      run_full_sta(g, dfs_topo_sort(g).order, c, d);
      CHECK(matches(d, ref));
      const auto order = kahn_topo_sort(g).order;
      std::vector<Delay> push;
      propagate_arrival_push(a, order, c, push);
      CHECK(compare_values(push, ref.at).pass);

      const Levels L = levelize(g, order);
      for (int threads : {1, 2, 4}) {
        LevelPool pool(threads, threads == 2 ? BarrierKind::Block : BarrierKind::Spin);
        TimingState p;
        p.resize(el.num_nodes);
        propagate_arrival_parallel(g, L, c, p.at, pool);
        propagate_required_parallel(g, L, c, p.rt, pool);
        compute_slack(p);
        CHECK(matches(p, ref));
      }
    }
}

TEST(incremental_matches_reference_after_random_edits) {
  // Arrival, Required and Slack checked separately (plan §6.5), both modes, single and batched updates.
  for (IncMode mode : {IncMode::Conservative, IncMode::EarlyStop})
    for (const char* kind : kKinds)
      for (std::uint64_t seed = 1; seed <= 4; ++seed) {
        EdgeList el = generate({.kind = kind, .nodes = 2000, .seed = seed, .fanout = 2.2, .layers = 40});
        const TimingConstraints c{.default_required = 1500};
        CsrGraph g(el);
        IncrementalTimer t(g, c);
        Rng rng(seed + 99);
        for (int round = 0; round < 30; ++round) {
          const int batch = round % 3 == 0 ? 8 : 1;
          for (int b = 0; b < batch; ++b) {
            const auto e = static_cast<EdgeId>(rng.uniform(0, el.num_edges() - 1));
            const Delay d = round % 7 == 0 ? 0.0 : 150 * rng.unit();
            el.edges[e].delay = d;
            t.set_edge_delay(e, d);
          }
          t.update(mode);
          if (round % 5 == 4) {
            const TimingState ref = *oracle::reference_engine(el, c);
            CHECK(compare_values(t.state().at, ref.at).pass);
            CHECK(compare_values(t.state().rt, ref.rt).pass);
            CHECK(compare_values(t.state().slack, ref.slack).pass);
          }
        }
        // Reset restores the original graph exactly.
        const EdgeList orig = generate({.kind = kind, .nodes = 2000, .seed = seed, .fanout = 2.2, .layers = 40});
        t.reset(orig);
        CHECK(bit_equal(t.state(), *oracle::reference_engine(orig, c)));
      }
}

TEST(conservative_region_bounds_early_stop_work) {
  const EdgeList el = generate({.kind = "reconvergent", .nodes = 20000, .seed = 4, .layers = 100});
  CsrGraph g(el);
  IncrementalTimer t(g, {.default_required = 4000});
  Rng rng(1);
  for (int i = 0; i < 20; ++i) {
    for (int b = 0; b < 3; ++b) t.set_edge_delay(static_cast<EdgeId>(rng.uniform(0, el.num_edges() - 1)), 100 * rng.unit());
    const AffectedRegion region = t.affected_region();
    // Re-apply the same pending set under both modes via a copy of the graph state.
    const auto st = t.update(IncMode::EarlyStop);
    CHECK(st.forward_evals <= region.fwd);
    CHECK(st.backward_evals <= region.bwd);
    CHECK(st.changed_nodes <= region.either);
  }
}
