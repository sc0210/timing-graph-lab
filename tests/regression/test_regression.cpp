// Regression tests: pin generator output and timing results for fixed seeds, so any
// change to the RNG, generators or propagation semantics is caught explicitly.
// If a change is intentional, regenerate with `test_regression --print` and update kGolden.
#include <cstring>

#define main test_main_impl
#include "../test_main.hpp"
#undef main
#include "graph/csr_graph.hpp"
#include "graph/generators.hpp"
#include "graph/traversal.hpp"
#include "sta/incremental.hpp"
#include "sta/oracle.hpp"
#include "sta/required.hpp"

using namespace tgl;

namespace {
struct Golden {
  const char* kind;
  NodeId nodes;
  std::uint64_t seed;
  std::size_t edges;
  std::uint64_t graph_fp;
  std::uint64_t timing_checksum;
  Delay max_arrival;
};

const Golden kGolden[] = {
    // GOLDEN_BEGIN
    {"chain", 1000, 1, 999, 0x62bc83ca71d3d975ULL, 0x38c03d7d6b143611ULL, 0x1.896c20b35003ap+15},
    {"chain", 20000, 1, 19999, 0x00e43bd346cfd981ULL, 0x8f76fbb8a0c669ddULL, 0x1.eafee913c696fp+19},
    {"chain", 1000, 42, 999, 0xd10785fea7a52dfdULL, 0xfc5ae1a9aed101e0ULL, 0x1.843f34ec15457p+15},
    {"chain", 20000, 42, 19999, 0xc53f768f02b7b261ULL, 0xc6926bf5c619501fULL, 0x1.ec246512e93e6p+19},
    {"layered", 1000, 1, 3872, 0x33c4c7329d82dc80ULL, 0x2d39e3f52d49b91eULL, 0x1.535603ac5cbf5p+11},
    {"layered", 20000, 1, 79436, 0xbff2853bf1ec13abULL, 0x711fe01caf6e5839ULL, 0x1.88dd7fb761eb1p+13},
    {"layered", 1000, 42, 3872, 0xe406e3b1b4503ed9ULL, 0x653d455679353ab8ULL, 0x1.4b342d265240dp+11},
    {"layered", 20000, 42, 79436, 0xe1e98f9e1d3fa2bcULL, 0x42473f38413a67f2ULL, 0x1.87fdbfa6acb32p+13},
    {"reconvergent", 1000, 1, 3872, 0x4d9fed4d84d6a66fULL, 0x9e8be65a668f52c7ULL, 0x1.1f88ca51527d9p+11},
    {"reconvergent", 20000, 1, 79436, 0x73d9bc3a58d3739dULL, 0x6ab5e75ab1c62ff7ULL, 0x1.4ae8ab6872e29p+13},
    {"reconvergent", 1000, 42, 3872, 0x25c4fdaf276f42f5ULL, 0xc2993a2e76e74975ULL, 0x1.1b470fe63e947p+11},
    {"reconvergent", 20000, 42, 79436, 0x84ebae381e127ed2ULL, 0xd5196a2c9002c183ULL, 0x1.50956cd780d43p+13},
    {"random", 1000, 1, 4000, 0x921c9a3e5d588026ULL, 0xd12d6a9252de0008ULL, 0x1.1a63192045f96p+11},
    {"random", 20000, 1, 80000, 0x20bbf2e1819639f1ULL, 0x09c91a8fb836d037ULL, 0x1.c3779374ee9ddp+11},
    {"random", 1000, 42, 4000, 0x911c866eaa2c27dfULL, 0x2ef3f99f0f6667c5ULL, 0x1.1024924489161p+11},
    {"random", 20000, 42, 80000, 0x46206c1e54bc212dULL, 0x111bf32f1ada3808ULL, 0x1.06a7546312bep+12},
    // GOLDEN_END
};

struct Computed {
  std::size_t edges;
  std::uint64_t fp, cs;
  Delay max_at;
};

Computed compute(const char* kind, NodeId nodes, std::uint64_t seed) {
  const EdgeList el = generate({.kind = kind, .nodes = nodes, .seed = seed});
  CsrGraph g(el);
  TimingState s;
  run_full_sta(g, kahn_topo_sort(g).order, TimingConstraints{.default_required = 1000}, s);
  const auto sum = summarize(g, s);
  return {el.num_edges(), fingerprint(el), sum.checksum, sum.max_arrival};
}
}  // namespace

TEST(golden_graphs_and_timing) {
  CHECK(std::size(kGolden) > 0);
  for (const Golden& gd : kGolden) {
    const Computed c = compute(gd.kind, gd.nodes, gd.seed);
    CHECK_EQ(c.edges, gd.edges);
    CHECK_EQ(c.fp, gd.graph_fp);
    CHECK_EQ(c.cs, gd.timing_checksum);
    CHECK_EQ(c.max_at, gd.max_arrival);
  }
}

// Bug found during development: a cycle with no source left every AT at -inf, so the
// relaxation oracle "converged" immediately and accepted a cyclic graph.
TEST(relaxation_oracle_rejects_sourceless_cycle) {
  CHECK(!oracle::relaxation_oracle(EdgeList{3, {{0, 1, 1}, {1, 2, 1}, {2, 0, 1}}}, {}).has_value());
  CHECK(!oracle::reference_engine(EdgeList{4, {{0, 1, 1}, {1, 2, 1}, {2, 1, 1}, {2, 3, 1}}}, {}).has_value());
}

// Bug found during development: FNV over raw double bits gave the same low 32 checksum
// bits for every graph of equal size with integer delays (low mantissa bits all zero).
TEST(timing_checksum_mixes_all_bits) {
  std::uint64_t low_bits[2];
  for (int i = 0; i < 2; ++i) {
    const EdgeList el = generate({.kind = "chain", .nodes = 1000, .seed = std::uint64_t(i + 1), .delay_dist = "int"});
    CsrGraph g(el);
    TimingState s;
    run_full_sta(g, kahn_topo_sort(g).order, TimingConstraints{.default_required = 1000}, s);
    low_bits[i] = summarize(g, s).checksum & 0xffffffffULL;
  }
  CHECK(low_bits[0] != low_bits[1]);
}

// Bug class guarded: an incremental update must re-evaluate from ALL fanin, not just the edited edge.
TEST(incremental_decrease_falls_back_to_other_path) {
  CsrGraph g(EdgeList{5, {{0, 2, 2}, {2, 4, 3}, {1, 3, 7}, {3, 4, 1}}});
  IncrementalTimer t(g, {.default_required = 10});
  t.set_edge_delay(3, 0);  // D->E 1 -> 0: AT(E) = max(5, 7) = 7, the other path does not take over
  t.update(IncMode::EarlyStop);
  CHECK_EQ(t.state().at[4], 7.0);
  t.set_edge_delay(2, 1);  // B->D 7 -> 1: now A->C->E (5) dominates
  t.update(IncMode::EarlyStop);
  CHECK_EQ(t.state().at[4], 5.0);
}

int main(int argc, char** argv) {
  if (argc > 1 && std::strcmp(argv[1], "--print") == 0) {
    for (const char* kind : {"chain", "layered", "reconvergent", "random"})
      for (std::uint64_t seed : {1, 42})
        for (NodeId nodes : {1000u, 20000u}) {
          const Computed c = compute(kind, nodes, seed);
          std::printf("    {\"%s\", %u, %llu, %zu, 0x%016llxULL, 0x%016llxULL, %a},\n", kind, nodes,
                      static_cast<unsigned long long>(seed), c.edges, static_cast<unsigned long long>(c.fp),
                      static_cast<unsigned long long>(c.cs), c.max_at);
        }
    return 0;
  }
  return test_main_impl(argc, argv);
}
