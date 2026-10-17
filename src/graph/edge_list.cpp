#include "graph/edge_list.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <sstream>
#include <stdexcept>

#include "graph/graph_stats.hpp"

namespace tgl {

bool validate(const EdgeList& el, std::string* error, bool allow_self_loops) {
  for (std::size_t i = 0; i < el.edges.size(); ++i) {
    const Edge& e = el.edges[i];
    const char* why = nullptr;
    if (e.src >= el.num_nodes || e.dst >= el.num_nodes) why = "node id out of range";
    else if (e.src == e.dst && !allow_self_loops) why = "self loop";
    else if (!std::isfinite(e.delay) || e.delay < 0) why = "delay must be finite and non-negative";
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
  std::unique_ptr<FILE, int (*)(FILE*)> f(std::fopen(path.c_str(), "r"), &std::fclose);
  if (!f) throw std::runtime_error("cannot read " + path);
  char magic[32] = {};
  int version = 0;
  EdgeList el;
  std::size_t m = 0;
  if (std::fscanf(f.get(), "%31s %d %u %zu", magic, &version, &el.num_nodes, &m) != 4 ||
      std::string(magic) != "tgl-edgelist" || version != 1)
    throw std::runtime_error(path + ": not a tgl-edgelist v1 file");
  el.edges.resize(m);
  for (Edge& e : el.edges)
    if (std::fscanf(f.get(), "%u %u %lf", &e.src, &e.dst, &e.delay) != 3)
      throw std::runtime_error(path + ": truncated edge list");
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
