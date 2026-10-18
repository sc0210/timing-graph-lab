#pragma once
// Canonical Edge List (plan §4.1): the single source of truth for every test graph.
// Every representation (Adjacency List, CSR) is built from the same EdgeList so
// comparisons share identical nodes, edges and delays (plan §4.4).
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace tgl {

using NodeId = std::uint32_t;
using EdgeId = std::uint32_t;
using Delay = double;

inline constexpr Delay kNegInf = -std::numeric_limits<Delay>::infinity();
inline constexpr Delay kPosInf = std::numeric_limits<Delay>::infinity();

struct Edge {
  NodeId src;
  NodeId dst;
  Delay delay;
};

// (neighbor, delay) pair produced by every representation's fanin()/fanout().
struct Arc {
  NodeId node;
  Delay delay;
};

// Plan §4.1 calls this GraphInput.
struct EdgeList {
  NodeId num_nodes = 0;
  std::vector<Edge> edges;

  std::size_t num_edges() const { return edges.size(); }
  std::size_t memory_bytes() const { return sizeof(*this) + edges.capacity() * sizeof(Edge); }
};
using GraphInput = EdgeList;

// Structural checks of plan §5.3 items 1, 2 and 5: ids in range, no self loops,
// finite non-negative delays. Acyclicity is checked separately (kahn_topo_sort / oracle).
bool validate(const EdgeList& el, std::string* error = nullptr, bool allow_self_loops = false);

// Order-sensitive hash of the edge list (plan §10 step 2: content hash of the input).
std::uint64_t fingerprint(const EdgeList& el);

// Text format "tgl-edgelist 1 / <nodes> <edges> / <src> <dst> <delay>..." with
// round-trip exact delays (%.17g), for saving benchmark inputs (plan §10 step 2).
void write_edge_list(const EdgeList& el, const std::string& path);
EdgeList read_edge_list(const std::string& path);

// Parses all of `s` as a decimal floating-point number, like std::from_chars: no leading
// whitespace or '+', no hexadecimal form; "inf" and "nan" are accepted (validate() rejects
// them as delays); overflow fails. Portable replacement for the floating-point
// std::from_chars overloads, which libc++ (Apple Clang) lacks before LLVM 20.
bool parse_double(std::string_view s, double& out);

// Parses the same format from memory. Untrusted input is safe: counts are bounded by
// max_nodes and by the input length (each edge needs at least 6 bytes), and anything
// after the declared edges other than whitespace is rejected. Throws std::runtime_error.
// Structural checks (ids in range, delays) are left to validate().
EdgeList parse_edge_list(std::string_view text, std::string_view name = "<memory>", NodeId max_nodes = NodeId{1} << 28);

}  // namespace tgl
