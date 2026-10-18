// Trace replay tests: the committed traces in tests/traces, randomly generated traces with a
// check after every update, and negative tests proving the replayer reports failures.
#include <filesystem>
#include <sstream>

#include "../test_main.hpp"
#include "graph/generators.hpp"
#include "sta/trace.hpp"

using namespace tgl;

#ifndef TGL_TRACE_DIR
#error "TGL_TRACE_DIR must point at tests/traces"
#endif

namespace {

void expect_pass(const TraceResult& r) {
  for (const auto& f : r.failures) std::fprintf(stderr, "  %s\n", f.c_str());
  CHECK(r.ok());
}

// A random inline trace: random DAG, interleaved single and batched edits in both modes,
// resets, and a `check` after every update.
std::string random_trace(std::uint64_t seed) {
  Rng rng(seed);
  const char* kinds[] = {"layered", "reconvergent", "random", "chain"};
  const EdgeList el = generate({.kind = kinds[seed % 4],
                                .nodes = static_cast<NodeId>(30 + rng.uniform(0, 300)),
                                .seed = seed,
                                .fanout = 1 + 3 * rng.unit(),
                                .layers = 6});
  std::ostringstream t;
  t.precision(17);
  t << "tgl-trace 1\nnodes " << el.num_nodes << "\n";
  for (const Edge& e : el.edges) t << "edge " << e.src << ' ' << e.dst << ' ' << e.delay << "\n";
  for (NodeId v = 0; v < el.num_nodes; v += 7) t << "arrival " << v << ' ' << 10 * rng.unit() << "\n";
  t << "check\n";
  for (int op = 0; op < 60 && !el.edges.empty(); ++op) {
    const auto k = rng.uniform(0, 9);
    if (k < 7) {
      const int batch = rng.unit() < 0.3 ? static_cast<int>(rng.uniform(2, 20)) : 1;
      for (int b = 0; b < batch; ++b)
        t << "set " << rng.uniform(0, el.num_edges() - 1) << ' ' << (rng.unit() < 0.1 ? 0.0 : 120 * rng.unit()) << "\n";
      t << "update " << (rng.unit() < 0.5 ? "early_stop" : "conservative") << "\ncheck\n";
    } else if (k < 9) {
      t << "update\ncheck\n";  // nothing pending
    } else {
      t << "reset\ncheck\n";
    }
  }
  return t.str();
}

}  // namespace

TEST(committed_traces_pass) {
  int n = 0;
  for (const auto& entry : std::filesystem::directory_iterator(TGL_TRACE_DIR)) {
    if (entry.path().extension() != ".trace") continue;
    ++n;
    expect_pass(run_trace_file(entry.path().string()));
  }
  CHECK(n >= 2);
}

TEST(random_traces_pass) {
  for (std::uint64_t seed = 1; seed <= 60; ++seed) {
    const TraceResult r = run_trace(random_trace(seed), "random-" + std::to_string(seed));
    expect_pass(r);
    CHECK(r.updates > 0 && r.checks > r.updates);
  }
}

TEST(replayer_reports_wrong_expectations) {
  const char* t = "tgl-trace 1\nnodes 2\nedge 0 1 5\nperiod 10\nexpect at 1 6\nexpect changed 1\n";
  const TraceResult r = run_trace(t, "neg");
  CHECK_EQ(r.failures.size(), 2u);
  CHECK(!r.failures.empty() && r.failures[0].find("neg:5:") == 0);
}

TEST(replayer_rejects_malformed_traces) {
  const char* bad[] = {
      "nodes 2\n",                                              // missing header
      "tgl-trace 2\n",                                          // wrong version
      "tgl-trace 1\nnodes 2\nedge 0 1 1\nfrobnicate\n",         // unknown command
      "tgl-trace 1\nnodes 2\nedge 0 1 1\nset 5 1\n",            // edge id out of range
      "tgl-trace 1\nnodes 2\nedge 0 1 1\nset 0 -1\n",           // negative delay
      "tgl-trace 1\nnodes 2\nedge 0 1 1\ncheck\nnodes 3\n",     // graph command after timing started
      "tgl-trace 1\nnodes 2\nedge 0 1 1\nedge 1 0 1\ncheck\n",  // cycle
      "tgl-trace 1\nnodes 2\nedge 0 2 1\ncheck\n",              // node id out of range
      "tgl-trace 1\ngraph does-not-exist.el\n",                 // missing graph file
      "tgl-trace 1\nnodes 2\nedge 0 1 1\nupdate sideways\n",    // bad update mode
  };
  for (const char* t : bad) {
    const TraceResult r = run_trace(t, "bad", TGL_TRACE_DIR);
    if (r.ok()) std::fprintf(stderr, "  accepted malformed trace:\n%s\n", t);
    CHECK(!r.ok());
  }
}
