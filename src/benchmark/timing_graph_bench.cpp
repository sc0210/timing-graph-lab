// timing_graph_bench: runs one experiment configuration and appends one CSV row per
// (representation, phase, algorithm, thread count, update set, repetition).
//
// Per run (plan §10 SOP, steps 2-7):
//   1. generate (or --input) the canonical edge list; structure checks (§5.3); stats
//   2. reference timing with the independent reference engine (+ relaxation oracle on small graphs)
//   3. per representation and repetition (after --warmup unmeasured ones):
//        traversal : topo_kahn (+ topo_indegree), topo_dfs, bfs
//        timing    : arrival (pull, push, pull over DFS order), required, slack
//        parallel  : levelize, pool_start, arrival_par, required_par per thread count
//        structure : edge_insert (List: append, CSR: rebuild)
//   4. incremental (CSR): update ratio x distribution x mode, with full_recompute baseline
//   Every timed result is compared with the reference; correctness_pass is written on the
//   same row, and the exit code is 2 if any comparison failed (Gate D).
#include <sys/resource.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "benchmark/timer.hpp"
#include "graph/adjacency_list.hpp"
#include "graph/csr_graph.hpp"
#include "graph/generators.hpp"
#include "graph/graph_stats.hpp"
#include "graph/traversal.hpp"
#include "sta/arrival.hpp"
#include "sta/compare.hpp"
#include "sta/incremental.hpp"
#include "sta/level_pool.hpp"
#include "sta/oracle.hpp"
#include "sta/required.hpp"

#ifndef TGL_BUILD_TYPE
#define TGL_BUILD_TYPE "unknown"
#endif
#ifndef TGL_CXX_FLAGS
#define TGL_CXX_FLAGS ""
#endif

namespace {

using namespace tgl;
using bench::Stopwatch;

struct Options {
  GenParams gen{.kind = "layered", .nodes = 100000, .seed = 20261017, .fanout = 4.0, .layers = 100};
  std::string input;  // load edge list instead of generating
  std::vector<std::string> reprs = {"list", "csr"};
  std::vector<std::string> algorithms = {"traversal", "timing", "parallel", "structure", "incremental"};
  std::vector<int> threads = {1, 2, 4};
  std::vector<std::string> barriers = {"spin", "block"};
  std::vector<double> update_ratios = {0.0, 0.01, 0.05, 0.10};
  std::vector<std::string> update_dists = {"localized", "distributed", "critical"};
  std::vector<std::string> inc_modes = {"conservative", "early_stop"};
  double insert_ratio = 0.01;
  int repetitions = 10;
  int warmup = 2;
  double source_arrival_spread = 0;  // a0 of each source uniform in [0, spread]
  double period = -1;                // sink required time; < 0 => max arrival
  std::size_t oracle_max_edges = 20000;
  Tolerance tol;
  std::string output;  // CSV path; empty = stdout
  std::string graph_stats_out, dump_graph;
  std::string commit = "unknown", run_id, notes;

  bool has(const std::string& a) const {
    return std::find(algorithms.begin(), algorithms.end(), a) != algorithms.end();
  }
};

void usage() {
  std::cerr <<
      R"(usage: timing_graph_bench [options]          (defaults follow plan §5.2)
 input
  --topology KIND        chain | layered | reconvergent | random      (layered)
  --nodes N              node count                                    (100000)
  --layers L             layered/reconvergent layer count, 0 = sqrt(N) (100)
  --span K               reconvergent: fanin from previous K layers    (4)
  --fanout F             target average fanout                         (4)
  --delay-min/--delay-max D   edge delay range                         (1, 100)
  --delay-dist real|int  delay distribution                            (real)
  --no-shuffle           keep generator node ids (already topologically sorted)
  --seed S               generator seed; update sets derive from it    (20261017)
  --input FILE           load a saved edge list instead of generating
  --dump-graph FILE      save the generated edge list (round-trip exact)
 experiment
  --representation R     both | list | csr (comma list)                (both)
  --algorithm A          all | comma list of traversal,timing,parallel,structure,incremental (all)
  --threads LIST         thread counts for parallel phases             (1,2,4)
  --barrier LIST         spin,block level barrier kinds                (both)
  --update-ratio LIST    incremental update ratios                     (0,0.01,0.05,0.10)
  --update-dist LIST     localized,distributed,critical                (all three)
  --inc-mode LIST        conservative,early_stop                       (both)
  --insert-ratio R       edges inserted in the structure phase, x|E|   (0.01)
  --source-arrival-spread X   a0 of each source uniform in [0, X]      (0)
  --period P             sink required time (default: max arrival)
  --repetitions R        measured repetitions                          (10)
  --warmup W             unmeasured warm-up repetitions                (2)
 validation
  --oracle-max-edges M   also run the O(VE) relaxation oracle when |E| <= M (20000)
  --tol-abs / --tol-rel  comparison tolerance (plan §7.4)              (1e-9, 1e-9)
 output
  --output FILE          append CSV rows (header written when the file is new)
  --graph-stats FILE     append one JSON line of graph statistics
  --commit SHA  --run-id ID  --notes TEXT    recorded in every row
)";
}

std::vector<std::string> split(const std::string& s) {
  std::vector<std::string> v;
  std::stringstream ss(s);
  for (std::string item; std::getline(ss, item, ',');)
    if (!item.empty()) v.push_back(item);
  return v;
}

Options parse(int argc, char** argv) {
  Options o;
  for (int i = 1; i < argc; ++i) {
    const std::string k = argv[i];
    auto val = [&]() -> std::string {
      if (i + 1 >= argc) throw std::invalid_argument("missing value for " + k);
      return argv[++i];
    };
    auto doubles = [&] {
      std::vector<double> v;
      for (auto& s : split(val())) v.push_back(std::stod(s));
      return v;
    };
    if (k == "--topology" || k == "--gen")
      o.gen.kind = val();
    else if (k == "--nodes")
      o.gen.nodes = static_cast<NodeId>(std::stoul(val()));
    else if (k == "--layers")
      o.gen.layers = static_cast<NodeId>(std::stoul(val()));
    else if (k == "--span")
      o.gen.span = static_cast<NodeId>(std::stoul(val()));
    else if (k == "--fanout")
      o.gen.fanout = std::stod(val());
    else if (k == "--delay-min")
      o.gen.delay_min = std::stod(val());
    else if (k == "--delay-max")
      o.gen.delay_max = std::stod(val());
    else if (k == "--delay-dist")
      o.gen.delay_dist = val();
    else if (k == "--no-shuffle")
      o.gen.shuffle = false;
    else if (k == "--seed")
      o.gen.seed = std::stoull(val());
    else if (k == "--input")
      o.input = val();
    else if (k == "--dump-graph")
      o.dump_graph = val();
    else if (k == "--representation") {
      const auto v = val();
      o.reprs = v == "both" ? std::vector<std::string>{"list", "csr"} : split(v);
    } else if (k == "--algorithm") {
      const auto v = val();
      if (v != "all") o.algorithms = split(v);
    } else if (k == "--threads") {
      o.threads.clear();
      for (auto& s : split(val())) o.threads.push_back(std::stoi(s));
    } else if (k == "--barrier")
      o.barriers = split(val());
    else if (k == "--update-ratio")
      o.update_ratios = doubles();
    else if (k == "--update-dist")
      o.update_dists = split(val());
    else if (k == "--inc-mode")
      o.inc_modes = split(val());
    else if (k == "--insert-ratio")
      o.insert_ratio = std::stod(val());
    else if (k == "--source-arrival-spread")
      o.source_arrival_spread = std::stod(val());
    else if (k == "--period")
      o.period = std::stod(val());
    else if (k == "--repetitions" || k == "--repeat")
      o.repetitions = std::stoi(val());
    else if (k == "--warmup")
      o.warmup = std::stoi(val());
    else if (k == "--oracle-max-edges")
      o.oracle_max_edges = std::stoull(val());
    else if (k == "--tol-abs")
      o.tol.abs = std::stod(val());
    else if (k == "--tol-rel")
      o.tol.rel = std::stod(val());
    else if (k == "--output" || k == "--out")
      o.output = val();
    else if (k == "--graph-stats")
      o.graph_stats_out = val();
    else if (k == "--commit")
      o.commit = val();
    else if (k == "--run-id")
      o.run_id = val();
    else if (k == "--notes")
      o.notes = val();
    else if (k == "-h" || k == "--help") {
      usage();
      std::exit(0);
    } else
      throw std::invalid_argument("unknown option " + k);
  }
  for (auto& r : o.reprs) {
    if (r == "adj") r = "list";
    if (r != "list" && r != "csr") throw std::invalid_argument("unknown representation " + r);
  }
  for (auto& a : o.algorithms)
    if (a != "traversal" && a != "timing" && a != "parallel" && a != "structure" && a != "incremental")
      throw std::invalid_argument("unknown algorithm " + a);
  for (auto& d : o.update_dists)
    if (d != "localized" && d != "distributed" && d != "critical")
      throw std::invalid_argument("unknown update dist " + d);
  for (auto& m : o.inc_modes)
    if (m != "conservative" && m != "early_stop") throw std::invalid_argument("unknown inc mode " + m);
  for (double r : o.update_ratios)
    if (r < 0 || r > 1) throw std::invalid_argument("update ratio must be in [0, 1]");
  for (int t : o.threads)
    if (t < 1) throw std::invalid_argument("thread count must be >= 1");
  for (auto& b : o.barriers)
    if (b != "spin" && b != "block") throw std::invalid_argument("unknown barrier " + b);
  if (o.repetitions < 1 || o.warmup < 0) throw std::invalid_argument("need repetitions >= 1, warmup >= 0");
  return o;
}

std::string cpu_model() {
  std::ifstream f("/proc/cpuinfo");
  for (std::string line; std::getline(f, line);)
    if (line.rfind("model name", 0) == 0) return line.substr(line.find(':') + 2);
  return "unknown";
}

// Process-wide peak RSS so far (not per phase; see docs/experiment_protocol.md).
long long peak_rss_bytes() {
  rusage ru{};
  getrusage(RUSAGE_SELF, &ru);
  return static_cast<long long>(ru.ru_maxrss) * 1024;  // ru_maxrss is KiB on Linux
}

std::string csv_escape(const std::string& s) {
  if (s.find_first_of(",\"\n") == std::string::npos) return s;
  std::string r = "\"";
  for (char ch : s) r += ch == '"' ? std::string("\"\"") : std::string(1, ch);
  return r + "\"";
}

std::string hex(std::uint64_t v) {
  char b[17];
  std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(v));
  return b;
}

// Per-graph columns shared by all rows.
struct GraphInfo {
  GraphStats stats;
  std::size_t depth = 0, lw_median = 0, lw_max = 0;
  std::uint64_t fp = 0;
};

struct Row {
  std::string repr, phase, algorithm;
  int threads = 1;
  int rep = 0;
  double update_ratio = 0;
  std::string update_dist;
  std::uint64_t update_seed = 0, updated_edges = 0, affected_nodes = 0, changed_nodes = 0;
  std::int64_t time_ns = 0;
  std::uint64_t work = 0;
  std::size_t mem_bytes = 0;
  CompareResult cmp;  // correctness_pass / bitwise_equal / max_abs_err
  std::uint64_t checksum = 0;
  std::string notes;
  long long peak_rss = 0;
};

class Report {
 public:
  Report(const Options& o, const GraphInfo& g) : o_(o), g_(g) {}
  bool measured = true;
  void add(Row r) {
    if (!measured) return;
    r.peak_rss = peak_rss_bytes();
    rows_.push_back(std::move(r));
  }
  const std::vector<Row>& rows() const { return rows_; }

  void write() const {
    std::ofstream file;
    std::ostream* os = &std::cout;
    if (!o_.output.empty()) {
      const bool fresh = !std::ifstream(o_.output).good();
      file.open(o_.output, std::ios::app);
      if (!file) throw std::runtime_error("cannot open " + o_.output);
      os = &file;
      if (fresh) header(*os);
    } else {
      header(*os);
    }
    const std::string cpu = csv_escape(cpu_model()), compiler = csv_escape("gcc-compatible " __VERSION__);
    const std::string flags = csv_escape(TGL_CXX_FLAGS);
    const std::string topology = o_.input.empty() ? o_.gen.kind : "file";
    char buf[64];
    for (const Row& r : rows_) {
      const double eps = r.time_ns > 0 ? double(g_.stats.edges) * 1e9 / double(r.time_ns) : 0;
      std::string notes = o_.notes;
      if (!r.notes.empty()) notes += (notes.empty() ? "" : ";") + r.notes;
      std::snprintf(buf, sizeof buf, "%.3f", r.time_ns / 1e3);
      *os << csv_escape(o_.run_id) << ',' << o_.commit << ',' << o_.gen.seed << ',' << r.update_seed << ',' << topology
          << ',' << g_.stats.nodes << ',' << g_.stats.edges << ',' << o_.gen.fanout << ',' << g_.stats.sources << ','
          << g_.stats.sinks << ',' << g_.stats.max_fanout << ',' << g_.stats.max_fanin << ',' << g_.depth << ','
          << g_.lw_median << ',' << g_.lw_max << ',' << r.repr << ',' << r.phase << ',' << r.algorithm << ','
          << r.threads << ',' << r.rep << ',' << r.update_ratio << ',' << r.update_dist << ',' << r.updated_edges << ','
          << r.affected_nodes << ',' << r.changed_nodes << ',' << buf << ',' << r.work << ',' << eps << ','
          << r.mem_bytes << ',' << r.peak_rss << ',' << (r.cmp.pass ? "true" : "false") << ','
          << (r.cmp.bitwise ? "true" : "false") << ',' << r.cmp.max_abs_err << ',' << hex(r.checksum) << ','
          << hex(g_.fp) << ',' << compiler << ',' << TGL_BUILD_TYPE << ',' << flags << ',' << cpu << ','
          << csv_escape(notes) << '\n';
    }
  }

 private:
  static void header(std::ostream& os) {
    os << "run_id,git_commit,seed,update_seed,topology,node_count,edge_count,fanout_target,sources,sinks,"
          "max_fanout,max_fanin,depth,level_width_median,level_width_max,representation,phase,algorithm,"
          "thread_count,rep,update_ratio,update_dist,updated_edges,affected_nodes,changed_nodes,runtime_us,work,"
          "edges_per_s,struct_mem_bytes,peak_rss_bytes,correctness_pass,bitwise_equal,max_abs_err,checksum,"
          "graph_fp,compiler,build_type,cxx_flags,cpu_model,notes\n";
  }
  const Options& o_;
  const GraphInfo& g_;
  std::vector<Row> rows_;
};

// Shared reference data for one graph.
struct Context {
  const EdgeList& el;
  const Options& o;
  const TimingConstraints& c;
  const TimingState& ref;
  Report& rep;
  std::vector<NodeId> first_kahn_order;  // Kahn order of the first representation, for cross-checks
};

CompareResult ok() { return {}; }
CompareResult fail() {
  CompareResult r;
  r.pass = r.bitwise = false;
  return r;
}

template <class G>
std::string repr_name();
template <>
std::string repr_name<AdjacencyList>() {
  return "list";
}
template <>
std::string repr_name<CsrGraph>() {
  return "csr";
}

// New edges for the structure phase: random forward pairs in topological order (keeps a DAG).
std::vector<Edge> insertion_set(const EdgeList& el, const std::vector<NodeId>& order, const Options& o) {
  std::vector<Edge> v;
  const NodeId n = el.num_nodes;
  if (n < 2) return v;
  const auto k = std::max<std::uint64_t>(1, std::llround(o.insert_ratio * double(el.num_edges())));
  Rng rng(o.gen.seed ^ 0x5eedC0DEULL);
  for (std::uint64_t i = 0; i < k; ++i) {
    auto a = rng.uniform(0, n - 2), b = rng.uniform(a + 1, n - 1);
    v.push_back({order[a], order[b], o.gen.delay_min + (o.gen.delay_max - o.gen.delay_min) * rng.unit()});
  }
  return v;
}

template <class G>
void bench_repr(Context& cx, int r) {
  const EdgeList& el = cx.el;
  const Options& o = cx.o;
  const TimingConstraints& c = cx.c;
  const std::string name = repr_name<G>();
  const std::uint64_t m = el.num_edges();
  Report& rep = cx.rep;

  Stopwatch sw_build;
  G g(el);
  const auto t_build = sw_build.elapsed_ns();
  {
    Row row{.repr = name,
            .phase = "build",
            .algorithm = name == "csr" ? "counting_sort" : "push_back",
            .rep = r,
            .time_ns = t_build,
            .work = m,
            .mem_bytes = g.memory_bytes()};
    if constexpr (std::is_same_v<G, CsrGraph>)
      row.notes = "update_index_bytes=" + std::to_string(g.update_index_bytes());
    rep.add(row);
  }

  Stopwatch sw_topo;
  TopoResult topo = kahn_topo_sort(g);
  const auto t_topo = sw_topo.elapsed_ns();
  if (!topo.is_dag) throw std::runtime_error("graph contains a cycle (Kahn)");
  const bool topo_ok = is_topological_order(g, topo.order);
  std::string order_note;
  if (cx.first_kahn_order.empty())
    cx.first_kahn_order = topo.order;
  else
    order_note = std::string("same_order_as_first_repr=") + (cx.first_kahn_order == topo.order ? "1" : "0");
  if (o.has("traversal")) {
    rep.add({.repr = name,
             .phase = "topo_indegree",
             .algorithm = "kahn",
             .rep = r,
             .time_ns = topo.init_ns,
             .work = el.num_nodes,
             .mem_bytes = topo.aux_bytes});
    rep.add({.repr = name,
             .phase = "topo_sort",
             .algorithm = "kahn",
             .rep = r,
             .time_ns = t_topo,
             .work = topo.work,
             .mem_bytes = topo.aux_bytes + topo.order.capacity() * sizeof(NodeId),
             .cmp = topo_ok ? ok() : fail(),
             .notes = order_note});

    Stopwatch sw_dfs;
    TopoResult dfs = dfs_topo_sort(g);
    const auto t_dfs = sw_dfs.elapsed_ns();
    const bool dfs_ok = dfs.is_dag && is_topological_order(g, dfs.order);
    rep.add({.repr = name,
             .phase = "topo_sort",
             .algorithm = "dfs",
             .rep = r,
             .time_ns = t_dfs,
             .work = dfs.work,
             .mem_bytes = dfs.aux_bytes + dfs.order.capacity() * sizeof(NodeId),
             .cmp = dfs_ok ? ok() : fail()});

    Stopwatch sw_bfs;
    TopoResult bfs = bfs_order(g);
    const auto t_bfs = sw_bfs.elapsed_ns();
    // BFS is measured as a traversal only; whether its visit order is a valid timing order is recorded in notes.
    rep.add({.repr = name,
             .phase = "traversal",
             .algorithm = "bfs",
             .rep = r,
             .time_ns = t_bfs,
             .work = bfs.work,
             .mem_bytes = bfs.aux_bytes + bfs.order.capacity() * sizeof(NodeId),
             .cmp = bfs.order.size() == el.num_nodes ? ok() : fail(),
             .notes = std::string("bfs_order_is_topological=") + (is_topological_order(g, bfs.order) ? "1" : "0")});

    if (o.has("timing") && dfs_ok) {
      // Experiment E part 2: same propagation, different topological order.
      std::vector<Delay> at;
      Stopwatch sw;
      propagate_arrival(g, dfs.order, c, at);
      const auto t = sw.elapsed_ns();
      rep.add({.repr = name,
               .phase = "arrival",
               .algorithm = "pull_dfs_order",
               .rep = r,
               .time_ns = t,
               .work = m,
               .cmp = compare_values(at, cx.ref.at, o.tol)});
    }
  }

  if (o.has("timing")) {
    TimingState s;
    s.resize(g.num_nodes());
    Stopwatch sw_at;
    propagate_arrival(g, topo.order, c, s.at);
    const auto t_at = sw_at.elapsed_ns();
    rep.add({.repr = name,
             .phase = "arrival",
             .algorithm = "pull",
             .rep = r,
             .time_ns = t_at,
             .work = m,
             .cmp = compare_values(s.at, cx.ref.at, o.tol)});

    std::vector<Delay> at_push;
    Stopwatch sw_push;
    propagate_arrival_push(g, topo.order, c, at_push);
    const auto t_push = sw_push.elapsed_ns();
    rep.add({.repr = name,
             .phase = "arrival",
             .algorithm = "push",
             .rep = r,
             .time_ns = t_push,
             .work = m,
             .cmp = compare_values(at_push, cx.ref.at, o.tol)});

    Stopwatch sw_rt;
    propagate_required(g, topo.order, c, s.rt);
    const auto t_rt = sw_rt.elapsed_ns();
    rep.add({.repr = name,
             .phase = "required",
             .algorithm = "pull",
             .rep = r,
             .time_ns = t_rt,
             .work = m,
             .cmp = compare_values(s.rt, cx.ref.rt, o.tol)});

    Stopwatch sw_sl;
    compute_slack(s);
    const auto t_sl = sw_sl.elapsed_ns();
    rep.add({.repr = name,
             .phase = "slack",
             .algorithm = "rt_minus_at",
             .rep = r,
             .time_ns = t_sl,
             .work = el.num_nodes,
             .cmp = compare_values(s.slack, cx.ref.slack, o.tol),
             .checksum = summarize(g, s).checksum});
  }

  if (o.has("parallel")) {
    Stopwatch sw_lv;
    const Levels levels = levelize(g, topo.order);
    const auto t_lv = sw_lv.elapsed_ns();
    rep.add({.repr = name,
             .phase = "levelize",
             .algorithm = "longest_hop",
             .rep = r,
             .time_ns = t_lv,
             .work = levels.num_levels(),
             .mem_bytes = (levels.offsets.capacity() + levels.nodes.capacity()) * sizeof(std::uint32_t)});
    for (const auto& bk : o.barriers)
      for (int t : o.threads) {
        const std::string alg = "level_parallel_" + bk;
        Stopwatch sw_pool;
        LevelPool pool(t, bk == "spin" ? BarrierKind::Spin : BarrierKind::Block);
        const auto t_pool = sw_pool.elapsed_ns();
        rep.add({.repr = name, .phase = "pool_start", .algorithm = alg, .threads = t, .rep = r, .time_ns = t_pool});
        TimingState p;
        p.resize(g.num_nodes());
        Stopwatch sw_pat;
        propagate_arrival_parallel(g, levels, c, p.at, pool);
        const auto t_pat = sw_pat.elapsed_ns();
        Stopwatch sw_prt;
        propagate_required_parallel(g, levels, c, p.rt, pool);
        const auto t_prt = sw_prt.elapsed_ns();
        rep.add({.repr = name,
                 .phase = "arrival",
                 .algorithm = alg,
                 .threads = t,
                 .rep = r,
                 .time_ns = t_pat,
                 .work = m,
                 .cmp = compare_values(p.at, cx.ref.at, o.tol)});
        rep.add({.repr = name,
                 .phase = "required",
                 .algorithm = alg,
                 .threads = t,
                 .rep = r,
                 .time_ns = t_prt,
                 .work = m,
                 .cmp = compare_values(p.rt, cx.ref.rt, o.tol)});
      }
  }

  if (o.has("structure")) {
    // Plan §4.3: structural modification cost measured separately from static processing.
    const auto extra = insertion_set(el, topo.order, o);
    std::int64_t t_ins = 0;
    std::size_t edges_after = 0, mem_after = 0;
    if constexpr (std::is_same_v<G, AdjacencyList>) {
      Stopwatch sw;
      for (const Edge& e : extra) g.add_edge(e.src, e.dst, e.delay);
      t_ins = sw.elapsed_ns();
      edges_after = g.num_edges();
      mem_after = g.memory_bytes();
    } else {
      EdgeList grown = el;  // the canonical edge list is kept; appending to it is not timed
      grown.edges.insert(grown.edges.end(), extra.begin(), extra.end());
      Stopwatch sw;
      CsrGraph rebuilt(grown);
      t_ins = sw.elapsed_ns();
      edges_after = rebuilt.num_edges();
      mem_after = rebuilt.memory_bytes();
    }
    rep.add({.repr = name,
             .phase = "edge_insert",
             .algorithm = name == "csr" ? "rebuild" : "append",
             .rep = r,
             .updated_edges = extra.size(),
             .time_ns = t_ins,
             .work = extra.size(),
             .mem_bytes = mem_after,
             .cmp = edges_after == m + extra.size() ? ok() : fail()});
  }
}

// ---- Experiment C: update sets ----

struct UpdateSet {
  std::uint64_t seed;
  std::vector<std::pair<EdgeId, Delay>> changes;
};

UpdateSet make_update_set(const EdgeList& el, const Options& o, const TimingState& base, const std::string& dist,
                          double ratio) {
  const std::size_t m = el.num_edges();
  const auto k = static_cast<std::size_t>(std::llround(ratio * double(m)));
  UpdateSet u;
  u.seed = o.gen.seed * 1000003ULL + std::hash<std::string>{}(dist) % 1000 * 1000 +
           static_cast<std::uint64_t>(std::llround(ratio * 1000));
  Rng rng(u.seed);
  std::vector<EdgeId> pick;
  if (k > 0 && dist == "distributed") {
    // Uniform sample without replacement (partial Fisher-Yates).
    std::vector<EdgeId> ids(m);
    std::iota(ids.begin(), ids.end(), EdgeId{0});
    for (std::size_t i = 0; i < k; ++i) std::swap(ids[i], ids[rng.uniform(i, m - 1)]);
    pick.assign(ids.begin(), ids.begin() + static_cast<std::ptrdiff_t>(k));
  } else if (k > 0 && dist == "localized") {
    // BFS over the undirected graph from a random node, taking edges in discovery order.
    std::vector<std::vector<EdgeId>> inc(el.num_nodes);
    for (EdgeId e = 0; e < m; ++e) inc[el.edges[e].src].push_back(e), inc[el.edges[e].dst].push_back(e);
    std::vector<std::uint8_t> seen_node(el.num_nodes, 0), seen_edge(m, 0);
    std::vector<NodeId> queue;
    while (pick.size() < k) {
      const auto start = static_cast<NodeId>(rng.uniform(0, el.num_nodes - 1));
      if (seen_node[start]) continue;
      seen_node[start] = 1;
      queue.assign(1, start);
      for (std::size_t h = 0; h < queue.size() && pick.size() < k; ++h)
        for (EdgeId e : inc[queue[h]]) {
          if (pick.size() == k) break;
          if (!seen_edge[e]) seen_edge[e] = 1, pick.push_back(e);
          const NodeId w = el.edges[e].src == queue[h] ? el.edges[e].dst : el.edges[e].src;
          if (!seen_node[w]) seen_node[w] = 1, queue.push_back(w);
        }
    }
  } else if (k > 0) {
    // critical: lowest edge slack RT(dst) - AT(src) - w first; random tie-break.
    std::vector<std::pair<Delay, std::uint64_t>> key(m);
    for (EdgeId e = 0; e < m; ++e) {
      const Edge& ed = el.edges[e];
      key[e] = {base.rt[ed.dst] - base.at[ed.src] - ed.delay, rng.next()};
    }
    std::vector<EdgeId> ids(m);
    std::iota(ids.begin(), ids.end(), EdgeId{0});
    std::partial_sort(ids.begin(), ids.begin() + static_cast<std::ptrdiff_t>(k), ids.end(),
                      [&](EdgeId a, EdgeId b) { return key[a] < key[b]; });
    pick.assign(ids.begin(), ids.begin() + static_cast<std::ptrdiff_t>(k));
  }
  for (EdgeId e : pick) u.changes.push_back({e, o.gen.delay_min + (o.gen.delay_max - o.gen.delay_min) * rng.unit()});
  return u;
}

void bench_incremental(const EdgeList& el, const Options& o, const TimingConstraints& c, const TimingState& ref,
                       Report& rep) {
  if (el.num_edges() == 0) return;
  CsrGraph g(el);
  IncrementalTimer timer(g, c);
  for (const auto& dist : o.update_dists)
    for (double ratio : o.update_ratios) {
      const UpdateSet u = make_update_set(el, o, ref, dist, ratio);
      // Reference for the modified graph, computed once by the independent engine.
      EdgeList modified = el;
      for (auto [e, d] : u.changes) modified.edges[e].delay = d;
      const TimingState mref = *oracle::reference_engine(modified, c);
      timer.reset(el);
      for (auto [e, d] : u.changes) timer.set_edge_delay(e, d);
      const AffectedRegion region = timer.affected_region();

      for (std::size_t mi = 0; mi < o.inc_modes.size(); ++mi) {
        const IncMode mode = o.inc_modes[mi] == "conservative" ? IncMode::Conservative : IncMode::EarlyStop;
        for (int it = 0; it < o.warmup + o.repetitions; ++it) {
          rep.measured = it >= o.warmup;
          const int r = it - o.warmup;
          timer.reset(el);  // plan §9.3 item 4: same initial delays and timing state every repetition
          Stopwatch sw;
          for (auto [e, d] : u.changes) timer.set_edge_delay(e, d);
          const IncrementalStats st = timer.update(mode);
          const auto t_inc = sw.elapsed_ns();
          const TimingState inc = timer.state();
          CompareResult cmp = compare_timing(inc, mref, o.tol);

          char notes[200];
          std::snprintf(
              notes, sizeof notes,
              "affected_fwd=%llu;affected_bwd=%llu;at_changed=%llu;rt_changed=%llu;evals_fwd=%llu;evals_bwd=%llu",
              (unsigned long long)region.fwd, (unsigned long long)region.bwd, (unsigned long long)st.at_changed,
              (unsigned long long)st.rt_changed, (unsigned long long)st.forward_evals,
              (unsigned long long)st.backward_evals);
          rep.add({.repr = "csr",
                   .phase = "incremental",
                   .algorithm = o.inc_modes[mi],
                   .rep = r,
                   .update_ratio = ratio,
                   .update_dist = dist,
                   .update_seed = u.seed,
                   .updated_edges = u.changes.size(),
                   .affected_nodes = region.either,
                   .changed_nodes = st.changed_nodes,
                   .time_ns = t_inc,
                   .work = st.forward_evals + st.backward_evals,
                   .cmp = cmp,
                   .notes = notes});

          if (mi == 0) {
            // Full Recompute baseline on the same modified delays (cached topological order).
            Stopwatch swf;
            timer.full_recompute();
            const auto t_full = swf.elapsed_ns();
            rep.add({.repr = "csr",
                     .phase = "full_recompute",
                     .algorithm = "pull",
                     .rep = r,
                     .update_ratio = ratio,
                     .update_dist = dist,
                     .update_seed = u.seed,
                     .updated_edges = u.changes.size(),
                     .affected_nodes = el.num_nodes,
                     .time_ns = t_full,
                     .work = 2 * el.num_edges(),
                     .cmp = compare_timing(timer.state(), mref, o.tol)});
          }
        }
      }
    }
  rep.measured = true;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    Options o = parse(argc, argv);
    if (o.run_id.empty())
      o.run_id = o.gen.kind + "-" + std::to_string(o.gen.nodes) + "-" + std::to_string(o.gen.seed) + "-" +
                 std::to_string(getpid());

    // 1. input + structure checks (plan §5.3)
    Stopwatch sw_gen;
    const EdgeList el = o.input.empty() ? generate(o.gen) : read_edge_list(o.input);
    const auto t_gen = sw_gen.elapsed_ns();
    if (std::string err; !validate(el, &err)) throw std::runtime_error("invalid graph: " + err);
    if (!o.dump_graph.empty()) write_edge_list(el, o.dump_graph);

    GraphInfo info;
    info.stats = compute_stats(el);
    info.fp = fingerprint(el);
    {
      CsrGraph g(el);
      TopoResult topo = kahn_topo_sort(g);
      if (!topo.is_dag) throw std::runtime_error("graph contains a cycle");
      const Levels L = levelize(g, topo.order);
      std::vector<std::size_t> widths(L.num_levels());
      for (std::size_t k = 0; k < widths.size(); ++k) widths[k] = L.width(k);
      info.depth = widths.size();
      if (!widths.empty()) {
        info.lw_max = *std::max_element(widths.begin(), widths.end());
        std::nth_element(widths.begin(), widths.begin() + widths.size() / 2, widths.end());
        info.lw_median = widths[widths.size() / 2];
      }
    }
    Report rep(o, info);
    rep.add({.repr = "edgelist",
             .phase = "generate",
             .algorithm = o.input.empty() ? o.gen.kind : "file",
             .time_ns = t_gen,
             .work = el.num_edges(),
             .mem_bytes = el.memory_bytes()});

    // 2. constraints and reference timing
    TimingConstraints c;
    if (o.source_arrival_spread > 0) {
      Rng rng(o.gen.seed ^ 0xA0A0A0A0ULL);
      c.arrival_override.assign(el.num_nodes, 0);
      for (auto& a : c.arrival_override) a = o.source_arrival_spread * rng.unit();
    }
    c.default_required = 0;
    Stopwatch sw_ref;
    std::optional<TimingState> ref = oracle::reference_engine(el, c);
    if (!ref) throw std::runtime_error("reference engine: graph contains a cycle");
    Delay max_at = 0;
    for (Delay a : ref->at) max_at = std::max(max_at, a);
    c.default_required = o.period >= 0 ? o.period : max_at;
    ref = oracle::reference_engine(el, c);
    rep.add({.repr = "edgelist",
             .phase = "reference",
             .algorithm = "reference_engine",
             .time_ns = sw_ref.elapsed_ns(),
             .work = el.num_edges()});
    if (el.num_edges() <= o.oracle_max_edges) {
      Stopwatch sw;
      auto rel = oracle::relaxation_oracle(el, c);
      const auto t = sw.elapsed_ns();
      rep.add({.repr = "edgelist",
               .phase = "oracle_check",
               .algorithm = "relaxation",
               .time_ns = t,
               .work = el.num_edges(),
               .cmp = rel ? compare_timing(*ref, *rel, o.tol) : fail()});
    }
    if (!o.graph_stats_out.empty()) {
      std::ofstream f(o.graph_stats_out, std::ios::app);
      f << "{\"run_id\":\"" << o.run_id << "\",\"graph_fp\":\"" << hex(info.fp) << "\",\"depth\":" << info.depth
        << ",\"level_width_median\":" << info.lw_median << ",\"level_width_max\":" << info.lw_max
        << ",\"stats\":" << info.stats.to_json() << "}\n";
    }

    // 3. representations (warm-up repetitions are run but not recorded)
    Context cx{el, o, c, *ref, rep, {}};
    const bool per_repr = o.has("traversal") || o.has("timing") || o.has("parallel") || o.has("structure");
    for (const auto& name : o.reprs) {
      if (!per_repr) break;
      for (int it = 0; it < o.warmup + o.repetitions; ++it) {
        rep.measured = it >= o.warmup;
        if (name == "list")
          bench_repr<AdjacencyList>(cx, it - o.warmup);
        else
          bench_repr<CsrGraph>(cx, it - o.warmup);
      }
      rep.measured = true;
    }

    // 4. incremental
    if (o.has("incremental")) bench_incremental(el, o, c, *ref, rep);

    rep.write();
    std::size_t failed = 0;
    for (const Row& r : rep.rows()) failed += !r.cmp.pass;
    if (failed) std::cerr << "VERIFY FAILED: " << failed << " row(s) with correctness_pass=false\n";
    return failed ? 2 : 0;
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << "\n(--help for usage)\n";
    return 1;
  }
}
