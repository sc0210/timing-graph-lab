// timing_graph_sta: full static timing on one graph, with per-node results.
//
//   timing_graph_sta INPUT [options]
//     INPUT                  an ISCAS .bench netlist (by extension) or a tgl-edgelist file
//     --unit-delay D         .bench: delay of every gate (default 1)
//     --gate-delay TYPE=D    .bench: delay of one gate type, e.g. NOT=0.5 (repeatable)
//     --per-fanout D         .bench: extra delay per fanout of the gate output (default 0)
//     --period P             sink required time (default: max arrival, so WNS = 0)
//     --out FILE             per-node CSV: node,name,at,rt,slack (%.17g, exact round trip)
//     --write-edgelist FILE  the timing graph as a tgl-edgelist file
//     --write-names FILE     node id -> name, one per line
//
// Prints a summary: nodes, edges, sources, sinks, max arrival, WNS, TNS and the worst sink.
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>

#include "graph/bench_netlist.hpp"
#include "graph/csr_graph.hpp"
#include "graph/traversal.hpp"
#include "sta/required.hpp"

namespace {

[[noreturn]] void usage(int code) {
  std::fprintf(code ? stderr : stdout,
               "usage: timing_graph_sta INPUT [--unit-delay D] [--gate-delay TYPE=D] [--per-fanout D]\n"
               "                        [--period P] [--out FILE] [--write-edgelist FILE] [--write-names FILE]\n");
  std::exit(code);
}

bool ends_with(const std::string& s, const std::string& suffix) {
  return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

}  // namespace

int main(int argc, char** argv) {
  using namespace tgl;
  std::string input, out, edgelist_out, names_out;
  BenchDelayModel model;
  double period = -1;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto val = [&]() -> std::string {
      if (i + 1 >= argc) usage(2);
      return argv[++i];
    };
    if (a == "--unit-delay")
      model.unit = std::stod(val());
    else if (a == "--gate-delay") {
      const std::string v = val();
      const auto eq = v.find('=');
      if (eq == std::string::npos) usage(2);
      model.base[v.substr(0, eq)] = std::stod(v.substr(eq + 1));
    } else if (a == "--per-fanout")
      model.per_fanout = std::stod(val());
    else if (a == "--period")
      period = std::stod(val());
    else if (a == "--out")
      out = val();
    else if (a == "--write-edgelist")
      edgelist_out = val();
    else if (a == "--write-names")
      names_out = val();
    else if (a == "-h" || a == "--help")
      usage(0);
    else if (!a.empty() && a[0] == '-')
      usage(2);
    else
      input = a;
  }
  if (input.empty()) usage(2);

  try {
    EdgeList el;
    std::vector<std::string> names;
    if (ends_with(input, ".bench")) {
      BenchTimingGraph b = to_timing_graph(read_bench(input), model);
      std::printf("netlist: %zu gates, %zu flip-flops\n", b.num_gates, b.num_dffs);
      el = std::move(b.graph);
      names = std::move(b.names);
    } else {
      el = read_edge_list(input);
      for (NodeId v = 0; v < el.num_nodes; ++v) names.push_back(std::to_string(v));
    }
    if (std::string err; !validate(el, &err)) throw std::runtime_error("invalid graph: " + err);
    const CsrGraph g(el);
    const TopoResult topo = kahn_topo_sort(g);
    if (!topo.is_dag) throw std::runtime_error("the timing graph has a cycle (combinational loop?)");

    TimingConstraints c;
    TimingState s;
    s.resize(g.num_nodes());
    propagate_arrival(g, topo.order, c, s.at);
    Delay max_at = 0;
    for (Delay a : s.at) max_at = std::max(max_at, a);
    c.default_required = period >= 0 ? period : max_at;
    run_full_sta(g, topo.order, c, s);
    const TimingSummary sum = summarize(g, s);

    NodeId sources = 0, sinks = 0, worst = 0;
    bool have_worst = false;
    for (NodeId v = 0; v < g.num_nodes(); ++v) {
      sources += g.fanin(v).empty();
      if (g.fanout(v).empty()) {
        ++sinks;
        if (!have_worst || s.slack[v] < s.slack[worst]) worst = v, have_worst = true;
      }
    }
    std::printf("graph: %u nodes, %zu edges, %u sources, %u sinks\n", g.num_nodes(), g.num_edges(), sources, sinks);
    std::printf("required time %.6g, max arrival %.6g, WNS %.6g, TNS %.6g\n", c.default_required, max_at, sum.wns,
                sum.tns);
    if (have_worst) std::printf("worst sink: %s (slack %.6g)\n", names[worst].c_str(), s.slack[worst]);

    if (!out.empty()) {
      std::FILE* f = std::fopen(out.c_str(), "w");
      if (!f) throw std::runtime_error("cannot write " + out);
      std::fprintf(f, "node,name,at,rt,slack\n");
      for (NodeId v = 0; v < g.num_nodes(); ++v)
        std::fprintf(f, "%u,%s,%.17g,%.17g,%.17g\n", v, names[v].c_str(), s.at[v], s.rt[v], s.slack[v]);
      std::fclose(f);
    }
    if (!edgelist_out.empty()) write_edge_list(el, edgelist_out);
    if (!names_out.empty()) {
      std::ofstream f(names_out);
      for (const auto& n : names) f << n << '\n';
    }
  } catch (const std::exception& e) {
    std::fprintf(stderr, "error: %s\n", e.what());
    return 1;
  }
  return 0;
}
