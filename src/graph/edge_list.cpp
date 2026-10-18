#include "graph/edge_list.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>

#include "graph/graph_stats.hpp"

namespace tgl {

bool validate(const EdgeList& el, std::string* error, bool allow_self_loops) {
  for (std::size_t i = 0; i < el.edges.size(); ++i) {
    const Edge& e = el.edges[i];
    const char* why = nullptr;
    if (e.src >= el.num_nodes || e.dst >= el.num_nodes)
      why = "node id out of range";
    else if (e.src == e.dst && !allow_self_loops)
      why = "self loop";
    else if (!std::isfinite(e.delay) || e.delay < 0)
      why = "delay must be finite and non-negative";
    if (why) {
      if (error) *error = "edge " + std::to_string(i) + ": " + why;
      return false;
    }
  }
  return true;
}

std::uint64_t fingerprint(const EdgeList& el) {
  std::uint64_t h = 1469598103934665603ULL;
  auto mix = [&h](const void* p, std::size_t n) {
    const auto* b = static_cast<const unsigned char*>(p);
    for (std::size_t i = 0; i < n; ++i) h = (h ^ b[i]) * 1099511628211ULL;
  };
  mix(&el.num_nodes, sizeof el.num_nodes);
  for (const Edge& e : el.edges) {
    mix(&e.src, sizeof e.src);
    mix(&e.dst, sizeof e.dst);
    mix(&e.delay, sizeof e.delay);
  }
  return h;
}

void write_edge_list(const EdgeList& el, const std::string& path) {
  std::unique_ptr<FILE, int (*)(FILE*)> f(std::fopen(path.c_str(), "w"), &std::fclose);
  if (!f) throw std::runtime_error("cannot write " + path);
  std::fprintf(f.get(), "tgl-edgelist 1\n%u %zu\n", el.num_nodes, el.num_edges());
  for (const Edge& e : el.edges) std::fprintf(f.get(), "%u %u %.17g\n", e.src, e.dst, e.delay);
}

EdgeList read_edge_list(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) throw std::runtime_error("cannot read " + path);
  std::ostringstream buf;
  buf << f.rdbuf();
  return parse_edge_list(buf.str(), path);
}

namespace {

// Whitespace-separated token scanner over a string_view.
class Tokens {
 public:
  Tokens(std::string_view text, std::string_view name) : s_(text), name_(name) {}

  std::string_view next(const char* what) {
    skip();
    if (pos_ >= s_.size()) fail(std::string("unexpected end of input, expected ") + what);
    const std::size_t b = pos_;
    while (pos_ < s_.size() && !space(s_[pos_])) ++pos_;
    return s_.substr(b, pos_ - b);
  }
  template <class T>
  T number(const char* what) {
    const std::string_view t = next(what);
    T v{};
    const auto [p, ec] = std::from_chars(t.data(), t.data() + t.size(), v);
    if (ec != std::errc() || p != t.data() + t.size())
      fail(std::string("invalid ") + what + " '" + std::string(t) + "'");
    return v;
  }
  bool at_end() {
    skip();
    return pos_ >= s_.size();
  }
  std::size_t remaining() const { return s_.size() - pos_; }
  [[noreturn]] void fail(const std::string& msg) const { throw std::runtime_error(std::string(name_) + ": " + msg); }

 private:
  static bool space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
  void skip() {
    while (pos_ < s_.size() && space(s_[pos_])) ++pos_;
  }
  std::string_view s_, name_;
  std::size_t pos_ = 0;
};

}  // namespace

EdgeList parse_edge_list(std::string_view text, std::string_view name, NodeId max_nodes) {
  Tokens t(text, name);
  if (t.next("magic") != "tgl-edgelist" || t.number<int>("version") != 1) t.fail("not a tgl-edgelist v1 file");
  EdgeList el;
  el.num_nodes = t.number<NodeId>("node count");
  const auto m = t.number<std::uint64_t>("edge count");
  if (el.num_nodes > max_nodes) t.fail("node count " + std::to_string(el.num_nodes) + " exceeds the limit");
  // Every edge line is at least "0 0 0" plus a separator: 6 bytes. This bounds the
  // allocation by the input size, so a forged header cannot request gigabytes.
  if (m > t.remaining() / 6 + 1) t.fail("edge count " + std::to_string(m) + " exceeds what the input can hold");
  el.edges.resize(m);
  for (Edge& e : el.edges) {
    e.src = t.number<NodeId>("edge source");
    e.dst = t.number<NodeId>("edge destination");
    e.delay = t.number<double>("edge delay");
  }
  if (!t.at_end()) t.fail("unexpected content after the declared edges");
  return el;
}

namespace {
std::size_t bucket(std::uint32_t d) {
  if (d <= 2) return d;
  if (d <= 4) return 3;
  if (d <= 8) return 4;
  if (d <= 16) return 5;
  if (d <= 32) return 6;
  return 7;
}
}  // namespace

GraphStats compute_stats(const EdgeList& el) {
  GraphStats s;
  s.nodes = el.num_nodes;
  s.edges = el.num_edges();
  std::vector<std::uint32_t> out(el.num_nodes, 0), in(el.num_nodes, 0);
  for (const Edge& e : el.edges) {
    ++out[e.src];
    ++in[e.dst];
  }
  for (NodeId v = 0; v < el.num_nodes; ++v) {
    s.sources += in[v] == 0;
    s.sinks += out[v] == 0;
    s.isolated += in[v] == 0 && out[v] == 0;
    s.max_fanout = std::max(s.max_fanout, out[v]);
    s.max_fanin = std::max(s.max_fanin, in[v]);
    ++s.fanout_hist[bucket(out[v])];
    ++s.fanin_hist[bucket(in[v])];
  }
  s.avg_fanout = el.num_nodes ? double(s.edges) / el.num_nodes : 0;
  return s;
}

std::string GraphStats::to_json() const {
  std::ostringstream o;
  o << "{\"nodes\":" << nodes << ",\"edges\":" << edges << ",\"sources\":" << sources << ",\"sinks\":" << sinks
    << ",\"isolated\":" << isolated << ",\"max_fanout\":" << max_fanout << ",\"max_fanin\":" << max_fanin
    << ",\"avg_fanout\":" << avg_fanout;
  for (auto [name, hist] : {std::pair{"fanout_hist", &fanout_hist}, std::pair{"fanin_hist", &fanin_hist}}) {
    o << ",\"" << name << "\":{";
    for (std::size_t b = 0; b < kBuckets.size(); ++b) o << (b ? "," : "") << '"' << kBuckets[b] << "\":" << (*hist)[b];
    o << '}';
  }
  o << '}';
  return o.str();
}

}  // namespace tgl
