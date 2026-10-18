// timing_graph_trace: replays incremental-timing traces (format in include/sta/trace.hpp)
// and checks every `check` / `expect` against the reference engine.
//
//   timing_graph_trace [--tol-abs X] [--tol-rel X] TRACE...
//
// Exit status 0 when every trace passes, 1 when any check fails, 2 on usage errors.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "sta/trace.hpp"

int main(int argc, char** argv) {
  tgl::Tolerance tol;
  std::vector<std::string> files;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if ((a == "--tol-abs" || a == "--tol-rel") && i + 1 < argc) {
      (a == "--tol-abs" ? tol.abs : tol.rel) = std::strtod(argv[++i], nullptr);
    } else if (a == "-h" || a == "--help") {
      std::printf("usage: timing_graph_trace [--tol-abs X] [--tol-rel X] TRACE...\n");
      return 0;
    } else if (!a.empty() && a[0] == '-') {
      std::fprintf(stderr, "unknown option %s\n", a.c_str());
      return 2;
    } else {
      files.push_back(a);
    }
  }
  if (files.empty()) {
    std::fprintf(stderr, "usage: timing_graph_trace [--tol-abs X] [--tol-rel X] TRACE...\n");
    return 2;
  }
  int failed = 0;
  for (const auto& f : files) {
    const tgl::TraceResult r = tgl::run_trace_file(f, tol);
    std::printf("[%s] %s: %zu commands, %zu updates, %zu checks\n", r.ok() ? " OK " : "FAIL", f.c_str(), r.commands,
                r.updates, r.checks);
    for (const auto& msg : r.failures) std::printf("  %s\n", msg.c_str());
    failed += !r.ok();
  }
  return failed ? 1 : 0;
}
