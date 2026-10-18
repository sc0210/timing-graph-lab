#pragma once
// ISCAS .bench gate-level netlists as timing graphs (real-circuit structure, plan §13).
//
//   INPUT(a)          primary input
//   OUTPUT(z)         primary output
//   y = NAND(a, b)    gate: AND, NAND, OR, NOR, XOR, XNOR, NOT, BUF, BUFF
//   q = DFF(d)        flip-flop (ISCAS-89)
//   # comment
//
// Mapping (one timing node per signal, plus capture nodes):
//   * each gate adds an edge from every input signal to its output signal, with the gate's delay;
//   * primary inputs and DFF outputs are launch points: sources (a0 applies);
//   * every primary output and every DFF input gets its own capture node `<signal>:po` /
//     `<q>:d` with a zero-delay edge, so it is a sink (r0 applies) even when the signal
//     also fans out, as at a flip-flop or output port in STA.
// The netlist must be combinationally acyclic once flip-flops are cut; otherwise
// to_timing_graph's result has a cycle, which kahn_topo_sort / validate callers detect.
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "graph/edge_list.hpp"

namespace tgl {

struct BenchGate {
  std::string type;  // upper case: AND, NAND, OR, NOR, XOR, XNOR, NOT, BUF, DFF
  std::string output;
  std::vector<std::string> inputs;
};

struct BenchNetlist {
  std::vector<std::string> inputs, outputs;
  std::vector<BenchGate> gates;  // DFFs included, type "DFF"
};

// Parses .bench text. Throws std::runtime_error with "<name>:<line>: ..." on syntax errors,
// unknown gate types, a signal driven twice, a used signal that is never driven, or a gate
// whose output is also one of its inputs (a one-gate combinational loop).
BenchNetlist parse_bench(std::string_view text, std::string_view name = "<bench>");
BenchNetlist read_bench(const std::string& path);

// Gate delay model: delay = base[type] (default `unit`) + per_fanout * fanout(output signal).
struct BenchDelayModel {
  double unit = 1.0;
  std::map<std::string, double> base;  // per gate type, e.g. {"NOT", 0.5}
  double per_fanout = 0.0;
};

struct BenchTimingGraph {
  EdgeList graph;
  std::vector<std::string> names;  // node id -> signal name (capture nodes: "<signal>:po", "<q>:d")
  std::size_t num_gates = 0, num_dffs = 0;
};

BenchTimingGraph to_timing_graph(const BenchNetlist& n, const BenchDelayModel& model = {});

}  // namespace tgl
