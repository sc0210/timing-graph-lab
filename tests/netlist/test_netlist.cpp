// .bench netlist import: hand-computed golden timing for ISCAS-85 c17 and a small
// sequential circuit, delay models, error handling, and every engine on imported graphs.
#include <array>
#include <map>

#include "../common.hpp"
#include "../engine_check.hpp"
#include "../test_main.hpp"
#include "graph/bench_netlist.hpp"

using namespace tgl;
using namespace tgl::test;

#ifndef TGL_BENCH_DIR
#error "TGL_BENCH_DIR must point at benchmarks/iscas"
#endif

namespace {

struct Named {
  std::map<std::string, Delay> at, rt, slack;
};

Named time_bench(const std::string& file, const BenchDelayModel& model = {}) {
  const BenchTimingGraph b = to_timing_graph(read_bench(std::string(TGL_BENCH_DIR) + "/" + file), model);
  const CsrGraph g(b.graph);
  const auto order = kahn_topo_sort(g).order;
  TimingConstraints c;
  std::vector<Delay> at;
  propagate_arrival(g, order, c, at);
  c.default_required = *std::max_element(at.begin(), at.end());
  TimingState s;
  run_full_sta(g, order, c, s);
  Named n;
  for (NodeId v = 0; v < g.num_nodes(); ++v) {
    n.at[b.names[v]] = s.at[v];
    n.rt[b.names[v]] = s.rt[v];
    n.slack[b.names[v]] = s.slack[v];
  }
  return n;
}

}  // namespace

TEST(c17_golden_unit_delay) {
  // Hand-computed: 10 = NAND(1,3) at 1, 11 = NAND(3,6) at 1, 16 = NAND(2,11) at 2,
  // 19 = NAND(11,7) at 2, 22 = NAND(10,16) at 3, 23 = NAND(16,19) at 3; required time 3.
  const Named n = time_bench("c17.bench");
  const std::map<std::string, std::array<Delay, 3>> expect = {
      {"1", {0, 1, 1}},  {"2", {0, 1, 1}},     {"3", {0, 0, 0}},    {"6", {0, 0, 0}},  {"7", {0, 1, 1}},
      {"10", {1, 2, 1}}, {"11", {1, 1, 0}},    {"16", {2, 2, 0}},   {"19", {2, 2, 0}}, {"22", {3, 3, 0}},
      {"23", {3, 3, 0}}, {"22:po", {3, 3, 0}}, {"23:po", {3, 3, 0}}};
  CHECK_EQ(n.at.size(), expect.size());
  for (const auto& [sig, v] : expect) {
    CHECK_EQ(n.at.at(sig), v[0]);
    CHECK_EQ(n.rt.at(sig), v[1]);
    CHECK_EQ(n.slack.at(sig), v[2]);
  }
}

TEST(c17_fanout_delay_model) {
  // delay = 1 + 0.5 * fanout(output): 11 and 16 drive two gates (2.0), 10 and 19 one gate
  // (1.5), 22 and 23 none (1.0). AT(22) = max(1.5, 2 + 2) + 1 = 5.
  const Named n = time_bench("c17.bench", {.unit = 1, .per_fanout = 0.5});
  CHECK_EQ(n.at.at("11"), 2.0);
  CHECK_EQ(n.at.at("16"), 4.0);
  CHECK_EQ(n.at.at("22"), 5.0);
  CHECK_EQ(n.at.at("23"), 5.0);
}

TEST(sequential_golden_flip_flops_cut_paths) {
  // q1, q2 (DFF outputs) are launch points at 0; n3 -> q1:d and y -> q2:d are captures.
  // n1 = AND(a,q1) 1, n2 = NOR(n1,b) 2, n3 = NOT(n2) 3, y = XOR(n2,q2) 3; required time 3.
  // n2 is also a primary output: its capture node n2:po has slack 1, but n2 itself is
  // critical through n3 and y.
  const Named n = time_bench("seq_example.bench");
  const std::map<std::string, std::array<Delay, 3>> expect = {
      {"a", {0, 0, 0}},    {"b", {0, 1, 1}},    {"q1", {0, 0, 0}},   {"q2", {0, 2, 2}},
      {"n1", {1, 1, 0}},   {"n2", {2, 2, 0}},   {"n3", {3, 3, 0}},   {"y", {3, 3, 0}},
      {"q1:d", {3, 3, 0}}, {"q2:d", {3, 3, 0}}, {"y:po", {3, 3, 0}}, {"n2:po", {2, 3, 1}}};
  CHECK_EQ(n.at.size(), expect.size());
  for (const auto& [sig, v] : expect) {
    CHECK_EQ(n.at.at(sig), v[0]);
    CHECK_EQ(n.rt.at(sig), v[1]);
    CHECK_EQ(n.slack.at(sig), v[2]);
  }
}

TEST(imported_graphs_pass_every_engine) {
  static EnginePools pools;
  for (const char* f : {"c17.bench", "seq_example.bench"}) {
    const BenchTimingGraph b = to_timing_graph(read_bench(std::string(TGL_BENCH_DIR) + "/" + f));
    const std::string err = check_all_engines(b.graph, {.default_required = 10}, pools);
    if (!err.empty()) std::fprintf(stderr, "  %s: %s\n", f, err.c_str());
    CHECK(err.empty());
  }
}

TEST(parser_rejects_malformed_netlists) {
  const char* bad[] = {
      "INPUT(a)\nINPUT(a)\n",                  // driven twice
      "INPUT(a)\nb = AND(a, c)\n",             // c never driven
      "INPUT(a)\nOUTPUT(z)\n",                 // output never driven
      "INPUT(a)\nb = MUX(a, a)\n",             // unknown gate
      "INPUT(a)\nb = NOT(a, a)\n",             // NOT with two inputs
      "INPUT(a)\nb = AND(a, a\n",              // missing parenthesis
      "INPUT(a b)\n",                          // invalid signal name
      "INPUT(a)\nb = AND(a, )\n",              // empty argument
      "WIRE(a)\n",                             // unknown declaration
      "INPUT(a)\nq = NAND(q)\n",               // one-gate combinational loop (found by fuzz_bench_parser)
      "INPUT(a)\nq = AND(a, q)\nOUTPUT(q)\n",  // the same, with another input
  };
  for (const char* t : bad) {
    bool threw = false;
    try {
      parse_bench(t);
    } catch (const std::runtime_error&) {
      threw = true;
    }
    if (!threw) std::fprintf(stderr, "  accepted malformed netlist:\n%s\n", t);
    CHECK(threw);
  }
}

TEST(dff_feeding_itself_is_sequential_not_a_loop) {
  const BenchTimingGraph b = to_timing_graph(parse_bench("INPUT(a)\nq = DFF(q)\nOUTPUT(q)\n"));
  CHECK(validate(b.graph));
  CHECK(kahn_topo_sort(CsrGraph(b.graph)).is_dag);
}

TEST(combinational_loop_becomes_a_cycle) {
  const BenchTimingGraph b = to_timing_graph(parse_bench("INPUT(a)\nx = AND(a, y)\ny = NOT(x)\nOUTPUT(y)\n"));
  CHECK(!kahn_topo_sort(CsrGraph(b.graph)).is_dag);
}

TEST(parser_accepts_iscas_variants) {
  // Lower-case gate names, BUFF, comments and blank lines, extra spaces.
  const auto n =
      parse_bench("# header\n\nINPUT( a )\nOUTPUT(z)\n  t = buff(a)  # trailing comment\nz = nand( t , a )\n");
  CHECK_EQ(n.gates.size(), 2u);
  CHECK_EQ(n.gates[0].type, std::string("BUF"));
  CHECK_EQ(n.gates[1].type, std::string("NAND"));
}
