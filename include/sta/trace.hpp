#pragma once
// Incremental-timing operation traces: a replayable sequence of delay edits, updates and
// checks, in the spirit of the TAU incremental timing contest's .ops files. A trace is a
// regression testbench for ordering-dependent incremental bugs that random batches miss.
//
// Format (one command per line; '#' starts a comment):
//
//   tgl-trace 1                  header (required, first line)
//   graph <file>                 load a tgl-edgelist file, relative to the trace's directory
//   nodes <n>                    ...or build the graph inline: node count,
//   edge <src> <dst> <delay>     ...then one line per edge (edge ids in order of appearance)
//   arrival <node> <a0>          per-source arrival time (default 0)
//   required <node> <r0>         per-sink required time
//   period <r0>                  default sink required time (default: max arrival of the initial graph)
//   --- the first command below starts the timer; graph commands are not allowed after it ---
//   set <edge> <delay>           change one edge delay (pending until update)
//   update [early_stop|conservative]   propagate pending edits (default early_stop)
//   reset                        restore every original delay and recompute
//   check                        compare AT, RT and slack of every node with the reference engine
//   expect <at|rt|slack> <node> <value>   exact value check (within the comparison tolerance)
//   expect changed <count>       number of nodes whose AT or RT changed in the last update
//
// `check` and `expect` before any update see the initial full timing.
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "sta/compare.hpp"

namespace tgl {

struct TraceResult {
  std::size_t commands = 0;  // executed commands (excluding header and graph commands)
  std::size_t checks = 0;    // `check` and `expect` commands evaluated
  std::size_t updates = 0;
  std::vector<std::string> failures;  // "<file>:<line>: <message>", empty when the trace passes
  bool ok() const { return failures.empty(); }
};

// Runs a trace given as text. `base_dir` resolves `graph` paths. Malformed traces
// (syntax, unknown command, out-of-range ids) are reported as failures, not exceptions.
TraceResult run_trace(std::string_view text, const std::string& name = "<trace>", const std::string& base_dir = ".",
                      const Tolerance& tol = {});

// Reads and runs a trace file.
TraceResult run_trace_file(const std::string& path, const Tolerance& tol = {});

}  // namespace tgl
