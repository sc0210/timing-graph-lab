# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses
[Semantic Versioning](https://semver.org/). Changes to generator output or CSV columns
are listed under **Changed**, because they break comparability with earlier results.

## [Unreleased]

### Added
- Optional oneTBB engines (`include/sta/tbb_parallel.hpp`), built when CMake finds TBB
  (`-DTGL_TBB=AUTO|ON|BUNDLED|OFF`, default `AUTO`):
  `--barrier tbb` (`level_parallel_tbb`, one `parallel_for` per level) and
  `--barrier tbb_dag` (`dag_parallel_tbb`, dependency-driven, no levels, with a new
  `dag_prepare` phase). Both are bit-identical to the serial engine and covered by the
  unit, differential and bench-smoke tests.
- `configs/parallel_tbb.json`; `analyze_results.py` and `plot_results.py` include the
  new engines; `.meta.json` records `tbb_engines`.
- CI installs oneTBB for the compiler, ASan and smoke jobs and keeps one job without it.
- `scripts/check_env.sh`: checks CMake, make/Ninja, a C++20 compiler (compiles and runs a
  `<barrier>`/`<span>` probe), Python, and optional oneTBB, matplotlib, git, clang-format;
  prints what is installed and missing with the install commands for Homebrew / apt / dnf /
  pacman, and offers to run them (`--check`, `--yes`, `--quiet`). `env.sh` gains `tgl_check`,
  and `tgl_build` runs the check first. All CI jobs run it.
- oneTBB v2022.3.0 as a git submodule (`third_party/oneTBB`): the repository builds the TBB
  engines without any system package. `TGL_TBB` gains `BUNDLED`; `AUTO` prefers a system
  TBB, then the submodule. TSan builds use the submodule compiled with `-fsanitize=thread`,
  so the TBB engines are now race-checked in CI. `check_env.sh` reports the submodule and
  offers `git submodule update --init` for a missing TBB; `.meta.json` records `tbb_source`.
- `.github/workflows/macos.yml`: README quick start on macOS 14 and 15 (with oneTBB installed
  by `check_env.sh --yes`, and without TBB), zsh/bash `env.sh`, and an ASan/UBSan job.

### Fixed
- Regression goldens failed on Apple Silicon: Clang contracts `a*b+c` into FMA by default
  on arm64, which changes rounding in the generators. All builds now use
  `-ffp-contract=off`; x86-64 results are unchanged (no FMA without `-march`).
- macOS: `peak_rss_bytes` multiplied `ru_maxrss` by 1024, but macOS reports bytes (1024× too
  large); CPU model, total memory and load average are now recorded on macOS too
  (`sysctl`, `os.getloadavg`) instead of `unknown`. `env.sh` no longer sets
  `detect_leaks=1` on macOS, where Apple's ASan does not support LeakSanitizer.
- `tests/unit` did not compile on macOS: POSIX `close(fd)` resolved to `tgl::close`
  because `<unistd.h>` was not included.

## [1.0.0] - 2026-10-17

First version that meets the plan's Definition of Done (§11):

- [x] Four graph topologies can be generated (chain, layered, reconvergent, random).
- [x] List and CSR both compute Arrival, Required and Slack.
- [x] All required correctness tests pass (T01–T14, property, differential, regression).
- [x] Incremental STA matches Full Recompute (both modes, every measured row).
- [x] The parallel version passes correctness and ThreadSanitizer data-race tests.
- [x] Every measurement is traceable to seed, parameters and Git commit.
- [x] CSV output plus three automatically generated analysis figures (§12.1).
- [x] The report separates measured results, hypotheses, interpretation and limitations (§12.2).

### Added
- `docs/report.md`: pilot experiment report, one §12.2-template section per experiment.
- `scripts/plot_results.py`: the three §12.1 figures (runtime vs size, parallel speedup,
  incremental speedup) with `--only` to draw each from its own suite; figures in
  `results/figures/`. CI draws figures 2 and 3 from the smoke suite.
- `env.sh`: sourceable shell environment (PATH, `TGL_*` variables, `tgl_*` helpers).
- Community files: license, contributing guide, code of conduct, security policy,
  issue and PR templates, CI workflow, `.clang-format`, `.editorconfig`, citation metadata.
- Scaling suite results (Experiment A, 10K–1M nodes).

### Changed
- Source formatted with `.clang-format`. No functional change; regression goldens are unchanged.

## [0.1.0] - 2026-10-17

### Added
- Canonical edge list, Adjacency List and SoA CSR representations.
- Deterministic chain / layered / reconvergent / random DAG generators.
- Kahn and DFS topological sorts, BFS, levelization.
- STA engine: pull and push arrival, required, slack; level-parallel propagation with spin
  and blocking barriers; incremental STA in conservative and early-stop modes.
- Independent oracles (path enumeration, relaxation, O(V+E) reference engine) and the
  §7.4 tolerance comparison.
- Unit (T01–T14), property, differential and regression suites; ASan/UBSan and TSan builds.
- `timing_graph_bench` CLI, suite runner with quality gates, analysis and plotting scripts.
- Pilot results for the baseline, incremental, parallel and scaling suites.

[Unreleased]: https://github.com/sc0210/timing-graph-lab/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/sc0210/timing-graph-lab/compare/v0.1.0...v1.0.0
[0.1.0]: https://github.com/sc0210/timing-graph-lab/releases/tag/v0.1.0
