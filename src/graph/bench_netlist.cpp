#include "graph/bench_netlist.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace tgl {

namespace {

std::string_view trim(std::string_view s) {
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
  return s;
}

bool valid_signal(std::string_view s) {
  return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) {
    return std::isalnum(c) || c == '_' || c == '.' || c == '[' || c == ']' || c == '$';
  });
}

const std::unordered_set<std::string>& gate_types() {
  static const std::unordered_set<std::string> t = {"AND", "NAND", "OR", "NOR", "XOR", "XNOR", "NOT", "BUF", "DFF"};
  return t;
}

}  // namespace

BenchNetlist parse_bench(std::string_view text, std::string_view name) {
  BenchNetlist n;
  std::unordered_set<std::string> driven;
  std::size_t line_no = 0, pos = 0;
  auto fail = [&](const std::string& msg) {
    throw std::runtime_error(std::string(name) + ":" + std::to_string(line_no) + ": " + msg);
  };
  auto drive = [&](const std::string& s) {
    if (!driven.insert(s).second) fail("signal '" + s + "' is driven more than once");
  };
  while (pos < text.size()) {
    const std::size_t nl = std::min(text.find('\n', pos), text.size());
    std::string_view line = text.substr(pos, nl - pos);
    pos = nl + 1;
    ++line_no;
    if (const auto hash = line.find('#'); hash != std::string_view::npos) line = line.substr(0, hash);
    line = trim(line);
    if (line.empty()) continue;

    const auto open = line.find('('), close = line.rfind(')');
    if (open == std::string_view::npos || close == std::string_view::npos || close < open ||
        trim(line.substr(close + 1)) != "")
      fail("expected 'INPUT(x)', 'OUTPUT(x)' or 'y = GATE(a, b, ...)'");
    const auto eq = line.find('=');
    std::string head(trim(
        line.substr(eq == std::string_view::npos ? 0 : eq + 1, open - (eq == std::string_view::npos ? 0 : eq + 1))));
    std::transform(head.begin(), head.end(), head.begin(), [](unsigned char c) { return std::toupper(c); });
    if (head == "BUFF") head = "BUF";
    std::vector<std::string> args;
    std::string_view rest = line.substr(open + 1, close - open - 1);
    while (true) {
      const auto comma = rest.find(',');
      const std::string_view a = trim(rest.substr(0, comma));
      if (!valid_signal(a)) fail("invalid signal name '" + std::string(a) + "'");
      args.emplace_back(a);
      if (comma == std::string_view::npos) break;
      rest = rest.substr(comma + 1);
    }

    if (eq == std::string_view::npos) {
      if (args.size() != 1) fail(head + " takes exactly one signal");
      if (head == "INPUT") {
        drive(args[0]);
        n.inputs.push_back(args[0]);
      } else if (head == "OUTPUT") {
        n.outputs.push_back(args[0]);
      } else {
        fail("unknown declaration '" + head + "'");
      }
      continue;
    }
    const std::string_view out = trim(line.substr(0, eq));
    if (!valid_signal(out)) fail("invalid signal name '" + std::string(out) + "'");
    if (!gate_types().count(head)) fail("unknown gate type '" + head + "'");
    if ((head == "NOT" || head == "BUF" || head == "DFF") && args.size() != 1) fail(head + " takes exactly one input");
    // A gate reading its own output is a one-gate combinational loop. It would become a
    // self-loop edge, which validate() rejects, so report it here. (Longer loops become
    // cycles that the topological sort detects; q = DFF(q) is sequential and fine.)
    if (head != "DFF" && std::find(args.begin(), args.end(), out) != args.end())
      fail("combinational loop: gate output '" + std::string(out) + "' is also its input");
    drive(std::string(out));
    n.gates.push_back({head, std::string(out), std::move(args)});
  }
  for (const auto& g : n.gates)
    for (const auto& in : g.inputs)
      if (!driven.count(in))
        throw std::runtime_error(std::string(name) + ": signal '" + in + "' is used but never driven");
  for (const auto& o : n.outputs)
    if (!driven.count(o)) throw std::runtime_error(std::string(name) + ": output '" + o + "' is never driven");
  return n;
}

BenchNetlist read_bench(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) throw std::runtime_error("cannot read " + path);
  std::ostringstream buf;
  buf << f.rdbuf();
  return parse_bench(buf.str(), path);
}

BenchTimingGraph to_timing_graph(const BenchNetlist& n, const BenchDelayModel& model) {
  BenchTimingGraph r;
  std::unordered_map<std::string, NodeId> id;
  auto node = [&](const std::string& s) {
    const auto [it, fresh] = id.emplace(s, static_cast<NodeId>(r.names.size()));
    if (fresh) r.names.push_back(s);
    return it->second;
  };
  // Node ids in a stable order: inputs, then gate outputs, then capture nodes.
  for (const auto& s : n.inputs) node(s);
  for (const auto& g : n.gates) node(g.output);

  std::unordered_map<std::string, std::size_t> fanout;
  for (const auto& g : n.gates)
    if (g.type != "DFF")
      for (const auto& in : g.inputs) ++fanout[in];
  for (const auto& g : n.gates) {
    if (g.type == "DFF") {
      ++r.num_dffs;
      continue;  // the DFF cuts the path: Q is a source, D gets a capture node below
    }
    ++r.num_gates;
    const auto b = model.base.find(g.type);
    const double d = (b == model.base.end() ? model.unit : b->second) + model.per_fanout * fanout[g.output];
    const NodeId out = id.at(g.output);
    for (const auto& in : g.inputs) r.graph.edges.push_back({id.at(in), out, d});
  }
  for (const auto& g : n.gates)
    if (g.type == "DFF") r.graph.edges.push_back({id.at(g.inputs[0]), node(g.output + ":d"), 0.0});
  for (const auto& o : n.outputs) r.graph.edges.push_back({id.at(o), node(o + ":po"), 0.0});
  r.graph.num_nodes = static_cast<NodeId>(r.names.size());
  return r;
}

}  // namespace tgl
