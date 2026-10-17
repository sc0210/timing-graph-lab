# Timing Graph Lab — Mini STA Benchmark Lab

[![CI](https://github.com/sc0210/timing-graph-lab/actions/workflows/ci.yml/badge.svg)](https://github.com/sc0210/timing-graph-lab/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![C++20](https://img.shields.io/badge/C%2B%2B-20-informational)
![Platform](https://img.shields.io/badge/platform-Linux%20x86--64-lightgrey)

A reproducible, measurable, verifiable lab for studying how timing-graph data
structures, graph traversal, static timing propagation and incremental STA affect
performance. This repository implements the experiment plan *EDA Timing Graph 實驗環境建置與驗證計劃書 v1.0*
(2026-10-17); section numbers below (§) refer to that plan.

Scope (phase 1, §1.1): a DAG with fixed non-negative edge delays and max-plus /
min-plus propagation. Liberty, SDC, clocks, setup/hold and parasitics are out of scope.

**Contents:** [Highlights](#highlights) · [Pilot findings](#pilot-findings) ·
[Quick start](#quick-start) · [Layout](#layout-23) · [Experiments](#experiments--configs) ·
[Output](#output-91) · [Design decisions](#design-decisions-and-deviations-from-the-plan) ·
[Contributing](#contributing) · [Citation](#citation) · [License](#license)

## Highlights

- **Correctness first.** Every measured row is re-verified against an independent O(V+E)
  reference engine. That engine is itself checked bit-for-bit against path enumeration and
  a relaxation oracle on 640 small cases. Benchmarks run only after the unit, property,
  differential, regression, ASan/UBSan and TSan gates pass.
- **Attributable timing.** Graph build, indegree init, topological sort, arrival,
  required, slack, levelization, thread start-up, incremental update and full recompute
  are timed as separate phases.
- **Reproducible.** Deterministic generators (own RNG), fixed seeds, input content hashes,
  per-run environment metadata and config SHA-256, all in CSV and JSON.
- **Covers the plan's experiment matrix.** Adjacency List vs CSR, four topologies,
  BFS/DFS/Kahn, incremental (conservative vs early-stop, three update distributions) and
  level-parallel propagation (spin vs blocking barrier).

## Pilot findings

From the first full run on a shared 4-vCPU cloud VM (details and caveats in
[`results/`](results/README.md)). Treat these as a pilot that exercises the pipeline,
not as definitive measurements.

| Question | Pilot answer |
|---|---|
| RQ1 List vs CSR | CSR uses 1.7–1.9× less memory, builds 1.2× faster and propagates 1.06–1.24× faster at 1M nodes ([scaling](results/scaling_report.md)) |
| RQ3 BFS vs topological order | BFS visit order is not a valid timing order on reconvergent and random DAGs; propagation uses Kahn or DFS orders ([baseline](results/baseline_report.md)) |
| RQ4 Incremental vs full | Early-stop wins 36–78× for 0.01 % non-critical edits, breaks even around 0.1 %, and loses from 1 % upward; edits on critical edges lose even at 0.01 % ([incremental](results/incremental_report.md)) |
| RQ5 Threads | Up to 2.6× at 4 threads on 1M-node graphs with the spin barrier. The chain (one node per level) is 4–50× slower than serial, depending on barrier and thread count. `std::barrier` costs ≈ 40–90 µs per level at 4 threads here ([parallel](results/parallel_report.md)) |
| RQ6 Correctness cost | 0 of 20,132 measured rows failed verification; all variants are bit-identical to the reference ([validation](docs/validation_report.md)) |

## Quick start

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
(cd build && ctest --output-on-failure)                 # Gate A + B

# Gate C: sanitizer builds (run_suite.py runs them automatically when present)
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug          -DTGL_SANITIZER=address && cmake --build build-asan -j
cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTGL_SANITIZER=thread  && cmake --build build-tsan -j

python3 scripts/run_suite.py configs/smoke.json           # ~1 min end-to-end check
python3 scripts/run_suite.py configs/baseline.json        # Experiment B + E (+ A at 100K)
python3 scripts/analyze_results.py results/baseline_*.csv --out results/baseline_report.md
```

### Shell environment

`env.sh` puts the benchmark binary and the scripts on `PATH` and defines helpers, so
everything can be run from any directory (bash or zsh):

```bash
source env.sh                     # uses ./build; or: TGL_BUILD_DIR=build-tsan source env.sh
tgl_build                         # configure + build (Release; sanitizer set for build-asan/build-tsan)
tgl_test                          # ctest in the current build dir (extra args go to ctest, e.g. -L unit)
timing_graph_bench --help         # the binary, now on PATH
tgl_suite smoke                   # = run_suite.py configs/smoke.json --build-dir $TGL_BUILD_DIR
analyze_results.py $TGL_RESULTS_DIR/smoke_*.csv
tgl_info                          # show paths and build type
tgl_unload                        # restore PATH and remove everything env.sh set
```

Exported variables: `TGL_ROOT`, `TGL_BUILD_DIR`, `TGL_BENCH`, `TGL_RESULTS_DIR`, plus
defaults for `ASAN_OPTIONS`, `UBSAN_OPTIONS`, `TSAN_OPTIONS` (halt on first report) and
`PYTHONDONTWRITEBYTECODE`. Values you already set are kept. Sourcing again is safe.

A single run:

```bash
./build/timing_graph_bench --topology layered --nodes 100000 --layers 100 --fanout 4 \
  --representation both --algorithm timing --threads 1,2,4 --seed 20261017 \
  --repetitions 10 --warmup 2 --output results/benchmark.csv
```

`./build/timing_graph_bench --help` lists every option. The defaults are the §5.2
baseline: 100,000 nodes, 100 layers, fanout target 4, seed 20261017, update ratios
0/1/5/10 %, threads 1/2/4, 10 measured repetitions after 2 warm-ups.

## Layout (§2.3)

| Path | Contents |
|---|---|
| `include/graph/edge_list.hpp` | Canonical `EdgeList` / `GraphInput` (§4.1), validation, fingerprint, save/load |
| `include/graph/adjacency_list.hpp` | Adjacency List with in + out vectors (§4.2) |
| `include/graph/csr_graph.hpp` | SoA CSR: `out_offsets/out_dst/out_delay`, `in_offsets/in_src/in_delay` (§4.3) |
| `include/graph/generators.hpp` | Deterministic chain / layered / reconvergent / random DAGs (§5) |
| `include/graph/graph_stats.hpp` | Sources, sinks, fanout/fanin histograms (§5.3 item 7) |
| `include/graph/traversal.hpp` | Kahn (with indegree-init timing), DFS topo sort, BFS, levelization (§6.1) |
| `include/sta/arrival.hpp`, `required.hpp` | Pull / push arrival, required, slack, level-parallel versions (§6.2, §6.3, §6.6) |
| `include/sta/level_pool.hpp` | Fixed worker pool, one barrier per level; spin or blocking barrier (§6.6) |
| `include/sta/incremental.hpp` | Incremental STA, conservative and early-stop modes (§6.4, §6.5) |
| `include/sta/oracle.hpp` | Path-enumeration and relaxation oracles, independent O(V+E) reference engine (§7.3) |
| `include/sta/compare.hpp` | Tolerance comparison ε_abs + ε_rel·max(\|x\|,\|y\|), NaN/∞ checks (§7.4) |
| `src/benchmark/timing_graph_bench.cpp` | Experiment driver / CLI (§10.1), CSV writer (§9.1) |
| `tests/{unit,property,differential,regression}` | L1–L4 test levels (§7.1); T01–T14 in `tests/unit` (§7.2) |
| `scripts/run_suite.py` | SOP runner: gates → matrix of runs → CSV + `.meta.json` + `.graphs.jsonl` (§10) |
| `scripts/analyze_results.py` | Gate D filter, per-experiment tables (stdlib only) |
| `scripts/plot_results.py` | Scaling / incremental / speedup plots (needs matplotlib) |
| `env.sh` | Sourceable shell environment: PATH, variables, `tgl_*` helpers |
| `.github/` | CI workflow (GCC/Clang, ASan/UBSan, TSan, clang-format, smoke suite), issue and PR templates |
| `configs/*.json` | `smoke`, `baseline` (Exp. B/E), `scaling` (Exp. A), `parallel` (Exp. D), `incremental` (Exp. C) |
| `docs/experiment_protocol.md` | What exactly each phase times, statistics, environment recording |
| `docs/validation_report.md` | Test levels, gate results, bugs found, verification evidence |

## Experiments → configs

| Plan | Config | Varies |
|---|---|---|
| Exp. A — representation (A1–A4) | `scaling.json` | 10K / 100K / 500K / 1M nodes × list/CSR × 3 seeds |
| Exp. B — topology | `baseline.json` | chain / layered / reconvergent / random at 100K, 3 seeds |
| Exp. C — incremental | `incremental.json` | ratio 0, 0.01, 0.1, 1, 5, 10 % × localized/distributed/critical × conservative/early-stop |
| Exp. D — threads | `parallel.json` | 1/2/4 threads × spin/blocking barrier, 100K and 1M (chain capped at 100K) |
| Exp. E — BFS/DFS/Kahn | `baseline.json` | traversal cost + same propagation over Kahn vs DFS orders |

## Output (§9.1)

One CSV row per (representation, phase, algorithm, threads, update set, repetition).
Main columns: `run_id, git_commit, seed, update_seed, topology, node_count, edge_count,
representation, phase, algorithm, thread_count, update_ratio, update_dist, updated_edges,
affected_nodes, changed_nodes, runtime_us, peak_rss_bytes, correctness_pass, compiler,
build_type, cpu_model, notes`, plus `work`, `edges_per_s`, `struct_mem_bytes`,
`bitwise_equal`, `max_abs_err`, `checksum`, `graph_fp` (input content hash) and graph
shape columns (`sources, sinks, max_fanout, max_fanin, depth, level_width_*`).
`run_suite.py` adds a `.meta.json` (OS, kernel, CPU, cores, RAM, compiler, flags,
governor, load, config SHA-256, gate results, failed points with reasons) and a
`.graphs.jsonl` with fanout/fanin histograms.

## Design decisions and deviations from the plan

- **Arc layout.** The Adjacency List stores `{neighbor, delay}` (16 bytes, same size as
  the plan's `Edge`), since the owning node is implicit. The CSR follows §4.3 exactly
  (SoA, `uint64_t` offsets). The CSR's edge-id → position maps, which exist only for
  in-place delay updates, are excluded from `struct_mem_bytes` and reported in `notes`.
- **Reverse index.** Both representations always build in- and out-arcs, so the
  `build` time includes the reverse index for both (§4.4).
- **Arrival direction.** The engine's default is *pull* (max over fanin); the plan's
  *push* pseudo-code (§6.2) is implemented and measured as `algorithm=push`.
- **Fanout target.** `--fanout F` is the mean fanin drawn per non-source node (layered,
  reconvergent) or |E| = F·|V| (random). The generated edge count is always recorded;
  for layered graphs it is about F·(|V| − first layer).
- **Delays** are real-valued (uniform in [1, 100)) by default, generated from the seed
  with a self-contained RNG; `--delay-dist int` gives integer delays.
- **Exactness.** All engines (pull, push, level-parallel, incremental) produce
  bit-identical results, because max/min never round and every candidate is the same
  sum. Correctness is still judged with the §7.4 tolerance; bitwise equality is
  reported separately in `bitwise_equal` and is not required.
- **Barriers.** `std::barrier` sleeps in the kernel and costs ~40 µs per level at 4
  threads on the development machine, so a spin barrier is also provided. Both are
  measured (`level_parallel_spin`, `level_parallel_block`).
- **Hardware counters.** `run_suite.py --perf` wraps runs with `perf stat` when it is
  installed. It was not available in the development container, so no cache-locality
  claims are made from these results (§9.3).

## Contributing

Contributions are welcome: bug reports, results from other machines, new experiments and
documentation fixes. Please read [CONTRIBUTING.md](CONTRIBUTING.md) first. It explains the
correctness and fair-comparison rules every change must follow. Use the
[issue templates](https://github.com/sc0210/timing-graph-lab/issues/new/choose) for bugs,
proposals and benchmark results. This project follows a [Code of Conduct](CODE_OF_CONDUCT.md).
Report security issues privately as described in [SECURITY.md](SECURITY.md). Changes are
tracked in [CHANGELOG.md](CHANGELOG.md).

## Citation

If you use this lab or its results, please cite it using the metadata in
[CITATION.cff](CITATION.cff) (GitHub's *Cite this repository* button).

## License

[MIT](LICENSE) © 2026 sc0210 and Timing Graph Lab contributors.
