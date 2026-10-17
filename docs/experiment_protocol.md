# Experiment protocol

This document fixes *what* is measured and *how*, so that results from different
people, commits and machines are comparable (plan §9, §10).

## 1. SOP (§10) and where each step is implemented

| Step | Implementation |
|---|---|
| 1. Experiment setting | `configs/<suite>.json`: `matrix` (or `matrices`) of topology, nodes, layers, fanout, seed, representation, algorithm, threads, barrier, update ratio/dist, inc mode; `repetitions`, `warmup` |
| 2. Generate and save input | `generate()` in the benchmark process; `graph_fp` (content hash) in every row; `--dump-graph FILE` saves the full edge list, `--input FILE` reloads it |
| 3. Structure checks | `validate()` (ids in range, no self-loops, finite non-negative delays), Kahn acyclicity check, `compute_stats()` → `.graphs.jsonl` |
| 4. Correctness suite | `run_suite.py` rebuilds and runs `ctest` in `build/` (Gates A+B), `build-asan/`, `build-tsan/` (Gate C); any failure aborts the suite |
| 5. Warm-up | `--warmup W` (default 2) full repetitions run and discarded |
| 6. Measurement | `--repetitions R` (default 10); one CSV row per measured phase |
| 7. Re-verification | every timed result is compared with the reference engine in the same process; `correctness_pass`, `bitwise_equal`, `max_abs_err` written on the same row; exit code 2 on failure |
| 8. Report | `analyze_results.py` (median, p90, min, max, CV, seed spread, speedups); `plot_results.py` |
| 9. Archive | CSV + `.meta.json` (config + SHA-256, commit, environment, gates) + `.graphs.jsonl`; commit the analysis with the code |

## 2. Timing boundaries

All times use `std::chrono::steady_clock`. Graph generation and the reference
computation happen before any measured phase. Each repetition rebuilds the
representation from the canonical edge list, so the phases of a repetition run on
freshly allocated structures.

| phase / algorithm | Timed region | Not timed |
|---|---|---|
| `generate` | generator call (once per run) | validation, statistics |
| `build` / `push_back`, `counting_sort` | constructing the representation, both directions | — |
| `topo_indegree` / `kahn` | indegree initialization inside Kahn | — |
| `topo_sort` / `kahn` | whole Kahn sort, including indegree init and the cycle check | `is_topological_order` validation |
| `topo_sort` / `dfs` | iterative DFS incl. colors, explicit stack, back-edge detection, final reversal | validation |
| `traversal` / `bfs` | multi-source BFS from all sources | validation |
| `arrival` / `pull`, `push`, `pull_dfs_order` | one full arrival pass; push includes its −∞ / a0 initialization | comparison |
| `required` / `pull` | one full required pass | comparison |
| `slack` | RT − AT for all nodes | comparison |
| `levelize` | computing level buckets from a topological order | — |
| `pool_start` / `level_parallel_*` | creating the worker threads for one thread count | joining them |
| `arrival`, `required` / `level_parallel_*` | one parallel pass over all levels, pool already running | comparison |
| `edge_insert` / `append` (list) | inserting ⌈1 % · \|E\|⌉ forward edges with `add_edge` | — |
| `edge_insert` / `rebuild` (CSR) | rebuilding the CSR from the extended edge list | copying the edge list |
| `incremental` / `conservative`, `early_stop` | applying all edge-delay changes + one `update()` | reset, comparison, affected-region preview |
| `full_recompute` / `pull` | arrival + required + slack on the modified delays, cached topological order | comparison |

Derived quantities:

- **Edges/s** = |E| / phase time (`edges_per_s` column; only meaningful for full passes).
- **S_inc** = T_full_recompute / T_incremental for the same update set (§8 Exp. C).
- **Speedup(p)** = T₁ / T_p, where T₁ = serial pull arrival + required.
  **Efficiency(p)** = Speedup(p) / p. *Algorithm-only* = parallel arrival + required;
  *end-to-end* adds `levelize` and `pool_start` (§8 Exp. D).

## 3. Incremental experiment details (§8 Exp. C)

- Update count k = round(ratio · |E|). The same update set (fixed `update_seed`, a
  function of the graph seed, distribution and ratio) is used for every mode and
  repetition. New delays are drawn uniformly from the generator's delay range, so
  updates both increase and decrease delays.
- Distributions:
  - `distributed`: k distinct edges sampled uniformly.
  - `localized`: undirected BFS from a random node, taking edges in discovery order
    (another random start is used if the component is exhausted).
  - `critical`: the k edges with the lowest edge slack RT(dst) − AT(src) − w in the
    baseline timing (random tie-break). The name is a group label: a selected edge does
    not necessarily change the critical path.
- Every repetition starts from the same state: `IncrementalTimer::reset()` restores all
  original delays and recomputes (§9.3 item 4).
- Recorded per row: `updated_edges`; `affected_nodes` = |fanout cone of edited edges'
  destinations ∪ fanin cone of their sources| (the conservative region); `work` = nodes
  actually re-evaluated (AT + RT evaluations); `changed_nodes` = nodes whose AT or RT
  changed. `notes` splits these into forward/backward parts.

## 4. Statistics

- Primary statistic: median over repetitions; p90, min, max and CV are also reported.
  CV > 10 % (for medians above 100 µs) is flagged as noisy.
- With several seeds, the per-seed medians are aggregated by their median, and
  `seed spread` = (max − min) / median of the per-seed medians is reported.
- Gate D: rows with `correctness_pass=false` are excluded from every table and listed
  in the report's first section.

## 5. Memory

- `struct_mem_bytes`: computed size of the graph arrays (capacity × element size, plus
  `std::vector` headers for the Adjacency List). For traversal phases it is the
  auxiliary memory (indegree, colors, stack, queue) plus the output order.
- `peak_rss_bytes`: process-wide `getrusage` peak *so far* when the row was recorded.
  It covers the edge list, the reference engine and earlier phases, so it is **not** a
  per-phase measurement. Each configuration runs in its own process, so values are
  comparable across configurations. To compare one phase's memory, run it alone
  (`--algorithm` and `--representation` restrict a run).

## 6. Environment and threads

- Formal runs must use a Release build (`run_suite.py` warns otherwise) on an otherwise
  idle machine. `.meta.json` records CPU model, logical CPUs, RAM, OS / kernel,
  compiler and flags, CPU governor, load average at start and end, and the config hash.
- Threads: `LevelPool` creates `p − 1` `std::thread` workers plus the calling thread,
  once per (thread count, barrier kind) per repetition. Creation is timed as
  `pool_start` and reused for the arrival and required passes. Threads are not pinned.
  The spin barrier assumes p ≤ physical cores; with oversubscription use `block`.
- Per-level structure is recorded (`depth`, `level_width_median`, `level_width_max`),
  because barrier cost per level, not node count, bounds level-parallel speedup.

## 7. Comparing results across machines

Results from different CPU architectures or core counts are analyzed separately
(`cpu_model` column). A result whose commit is tagged `-dirty` is not reproducible
from the repository and must not be used for conclusions.
