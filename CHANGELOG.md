# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses
[Semantic Versioning](https://semver.org/). Changes to generator output or CSV columns
are listed under **Changed**, because they break comparability with earlier results.

## [Unreleased]

### Added
- `env.sh`: sourceable shell environment (PATH, `TGL_*` variables, `tgl_*` helpers).
- Community files: license, contributing guide, code of conduct, security policy,
  issue and PR templates, CI workflow, `.clang-format`, `.editorconfig`, citation metadata.

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

[Unreleased]: https://github.com/sc0210/timing-graph-lab/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/sc0210/timing-graph-lab/releases/tag/v0.1.0
