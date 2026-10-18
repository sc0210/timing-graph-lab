#include "sta/trace.hpp"

#include <charconv>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <unordered_map>

#include "graph/csr_graph.hpp"
#include "sta/incremental.hpp"
#include "sta/oracle.hpp"

namespace tgl {

namespace {

std::vector<std::string_view> split_words(std::string_view line) {
  std::vector<std::string_view> w;
  std::size_t i = 0;
  while (i < line.size()) {
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r')) ++i;
    const std::size_t b = i;
    while (i < line.size() && line[i] != ' ' && line[i] != '\t' && line[i] != '\r') ++i;
    if (i > b) w.push_back(line.substr(b, i - b));
  }
  return w;
}

template <class T>
std::optional<T> parse_num(std::string_view s) {
  T v{};
  const auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
  if (ec != std::errc() || p != s.data() + s.size()) return std::nullopt;
  return v;
}

class Replayer {
 public:
  Replayer(std::string name, std::string base_dir, Tolerance tol)
      : name_(std::move(name)), base_dir_(std::move(base_dir)), tol_(tol) {}

  TraceResult run(std::string_view text) {
    std::size_t line_no = 0, pos = 0;
    bool header = false;
    while (pos <= text.size()) {
      const std::size_t nl = std::min(text.find('\n', pos), text.size());
      std::string_view line = text.substr(pos, nl - pos);
      pos = nl + 1;
      ++line_no;
      line_ = line_no;
      if (const auto hash = line.find('#'); hash != std::string_view::npos) line = line.substr(0, hash);
      const auto w = split_words(line);
      if (w.empty()) {
        if (nl == text.size()) break;
        continue;
      }
      if (!header) {
        if (w.size() != 2 || w[0] != "tgl-trace" || w[1] != "1") return fatal("missing 'tgl-trace 1' header");
        header = true;
        continue;
      }
      if (!command(w)) return std::move(res_);
      if (nl == text.size()) break;
    }
    if (!header) return fatal("empty trace (missing 'tgl-trace 1' header)");
    if (!timer_ && !start()) return std::move(res_);
    return std::move(res_);
  }

 private:
  // ---- reporting ----
  void fail(const std::string& msg) { res_.failures.push_back(name_ + ":" + std::to_string(line_) + ": " + msg); }
  TraceResult fatal(const std::string& msg) {
    fail(msg);
    return std::move(res_);
  }

  // ---- setup phase ----
  bool setup_command(const std::vector<std::string_view>& w) {
    const auto& cmd = w[0];
    if (cmd == "graph" && w.size() == 2) {
      if (inline_graph_) return fail("'graph' cannot be combined with 'nodes'/'edge'"), false;
      const std::filesystem::path p = std::filesystem::path(base_dir_) / std::string(w[1]);
      try {
        el_ = read_edge_list(p.string());
      } catch (const std::exception& e) {
        return fail(e.what()), false;
      }
      file_graph_ = true;
    } else if (cmd == "nodes" && w.size() == 2) {
      const auto n = parse_num<NodeId>(w[1]);
      if (!n || file_graph_) return fail("bad 'nodes' command"), false;
      el_.num_nodes = *n;
      inline_graph_ = true;
    } else if (cmd == "edge" && w.size() == 4) {
      const auto u = parse_num<NodeId>(w[1]), v = parse_num<NodeId>(w[2]);
      const auto d = parse_num<double>(w[3]);
      if (!u || !v || !d || !inline_graph_) return fail("bad 'edge' command (needs 'nodes' first)"), false;
      el_.edges.push_back({*u, *v, *d});
    } else if (cmd == "arrival" && w.size() == 3) {
      const auto v = parse_num<NodeId>(w[1]);
      const auto a = parse_num<double>(w[2]);
      if (!v || !a) return fail("bad 'arrival' command"), false;
      arrivals_[*v] = *a;
    } else if (cmd == "required" && w.size() == 3) {
      const auto v = parse_num<NodeId>(w[1]);
      const auto r = parse_num<double>(w[2]);
      if (!v || !r) return fail("bad 'required' command"), false;
      requireds_[*v] = *r;
    } else if (cmd == "period" && w.size() == 2) {
      const auto r = parse_num<double>(w[1]);
      if (!r) return fail("bad 'period' command"), false;
      period_ = *r;
    } else {
      return fail("unknown or malformed command '" + std::string(cmd) + "'"), false;
    }
    return true;
  }

  bool start() {
    if (std::string err; !validate(el_, &err)) return fail("invalid graph: " + err), false;
    for (auto* m : {&arrivals_, &requireds_})
      for (const auto& [v, _] : *m)
        if (v >= el_.num_nodes) return fail("constraint on node " + std::to_string(v) + " out of range"), false;
    if (!arrivals_.empty()) {
      c_.arrival_override.assign(el_.num_nodes, 0);
      for (const auto& [v, a] : arrivals_) c_.arrival_override[v] = a;
    }
    if (period_) {
      c_.default_required = *period_;
    } else {
      const auto s = oracle::reference_engine(el_, c_);
      if (!s) return fail("graph has a cycle"), false;
      Delay max_at = 0;
      for (Delay a : s->at) max_at = std::max(max_at, a);
      c_.default_required = max_at;
    }
    if (!requireds_.empty()) {
      c_.required_override.assign(el_.num_nodes, c_.default_required);
      for (const auto& [v, r] : requireds_) c_.required_override[v] = r;
    }
    original_ = el_;
    graph_ = std::make_unique<CsrGraph>(el_);
    try {
      timer_ = std::make_unique<IncrementalTimer>(*graph_, c_);
    } catch (const std::exception& e) {
      return fail(e.what()), false;
    }
    return true;
  }

  // ---- timing phase ----
  bool command(const std::vector<std::string_view>& w) {
    static const char* kSetup[] = {"graph", "nodes", "edge", "arrival", "required", "period"};
    const bool is_setup = std::find(std::begin(kSetup), std::end(kSetup), w[0]) != std::end(kSetup);
    if (!timer_) {
      if (is_setup) return setup_command(w);
      if (!start()) return false;
    } else if (is_setup) {
      return fail("'" + std::string(w[0]) + "' is only allowed before the first timing command"), false;
    }
    ++res_.commands;
    const auto& cmd = w[0];
    if (cmd == "set" && w.size() == 3) {
      const auto e = parse_num<EdgeId>(w[1]);
      const auto d = parse_num<double>(w[2]);
      if (!e || *e >= el_.num_edges()) return fail("edge id out of range"), false;
      if (!d || !(*d >= 0) || *d == kPosInf) return fail("delay must be finite and non-negative"), false;
      timer_->set_edge_delay(*e, *d);
      el_.edges[*e].delay = *d;  // el_ always holds the delays the timer has been told about
    } else if (cmd == "update" && w.size() <= 2) {
      IncMode mode = IncMode::EarlyStop;
      if (w.size() == 2) {
        if (w[1] == "conservative")
          mode = IncMode::Conservative;
        else if (w[1] != "early_stop")
          return fail("update mode must be early_stop or conservative"), false;
      }
      last_ = timer_->update(mode);
      applied_ = el_;
      ++res_.updates;
    } else if (cmd == "reset" && w.size() == 1) {
      timer_->reset(original_);
      el_ = original_;
      applied_ = original_;
      last_ = {};
    } else if (cmd == "check" && w.size() == 1) {
      ++res_.checks;
      const EdgeList& now = applied_ ? *applied_ : original_;
      const auto ref = oracle::reference_engine(now, c_);
      const CompareResult r = compare_timing(timer_->state(), *ref, tol_);
      if (!r.pass) {
        std::ostringstream o;
        o << "check failed: " << r.mismatches << " mismatching value(s), max |err| " << r.max_abs_err;
        fail(o.str());
      }
    } else if (cmd == "expect" && w.size() == 3 && w[1] == "changed") {
      ++res_.checks;
      const auto n = parse_num<std::uint64_t>(w[2]);
      if (!n) return fail("bad 'expect changed' count"), false;
      if (last_.changed_nodes != *n)
        fail("expected " + std::to_string(*n) + " changed node(s), got " + std::to_string(last_.changed_nodes));
    } else if (cmd == "expect" && w.size() == 4) {
      ++res_.checks;
      const auto v = parse_num<NodeId>(w[2]);
      const auto x = parse_num<double>(w[3]);
      if (!v || *v >= el_.num_nodes || !x) return fail("bad 'expect' command"), false;
      const TimingState& s = timer_->state();
      const std::vector<Delay>* vec = w[1] == "at"      ? &s.at
                                      : w[1] == "rt"    ? &s.rt
                                      : w[1] == "slack" ? &s.slack
                                                        : nullptr;
      if (!vec) return fail("expect target must be at, rt, slack or changed"), false;
      if (!close((*vec)[*v], *x, tol_)) {
        std::ostringstream o;
        o.precision(17);
        o << "expected " << w[1] << "(" << *v << ") = " << *x << ", got " << (*vec)[*v];
        fail(o.str());
      }
    } else {
      return fail("unknown or malformed command '" + std::string(cmd) + "'"), false;
    }
    return true;
  }

  std::string name_, base_dir_;
  Tolerance tol_;
  std::size_t line_ = 0;
  TraceResult res_;

  EdgeList el_, original_;
  std::optional<EdgeList> applied_;
  bool inline_graph_ = false, file_graph_ = false;
  std::unordered_map<NodeId, Delay> arrivals_, requireds_;
  std::optional<Delay> period_;
  TimingConstraints c_;
  std::unique_ptr<CsrGraph> graph_;
  std::unique_ptr<IncrementalTimer> timer_;
  IncrementalStats last_;
};

}  // namespace

TraceResult run_trace(std::string_view text, const std::string& name, const std::string& base_dir,
                      const Tolerance& tol) {
  return Replayer(name, base_dir, tol).run(text);
}

TraceResult run_trace_file(const std::string& path, const Tolerance& tol) {
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    TraceResult r;
    r.failures.push_back(path + ": cannot read trace");
    return r;
  }
  std::ostringstream buf;
  buf << f.rdbuf();
  return run_trace(buf.str(), path, std::filesystem::path(path).parent_path().string(), tol);
}

}  // namespace tgl
