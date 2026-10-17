# Results

Pilot run on 2026-10-17. Every suite passed Gates A–C (Release, ASan+UBSan and TSan
`ctest`) before measuring. Every measured row was re-verified against the reference engine.

| Suite (plan experiment) | Raw data | Report |
|---|---|---|
| baseline (Exp. B, E; A at 100K) | `baseline_20261017-152536.*` | [baseline_report.md](baseline_report.md) |
| incremental (Exp. C) | `incremental_20261017-152805.*` | [incremental_report.md](incremental_report.md) |
| parallel (Exp. D) | `parallel_20261017-153443.*` | [parallel_report.md](parallel_report.md) |
| scaling (Exp. A1–A4) | `scaling_20261017-154952.*` | [scaling_report.md](scaling_report.md) |

Figures (`figures/`, plan §12.1), each drawn from its own suite:

| Figure | Source | Command |
|---|---|---|
| [fig1_runtime_vs_size.png](figures/fig1_runtime_vs_size.png) | scaling | `plot_results.py results/scaling_*.csv --only fig1` |
| [fig2_parallel_speedup.png](figures/fig2_parallel_speedup.png) | parallel | `plot_results.py results/parallel_*.csv --only fig2` |
| [fig3_incremental_speedup.png](figures/fig3_incremental_speedup.png) | incremental | `plot_results.py results/incremental_*.csv --only fig3` |

The written analysis of all suites is [docs/report.md](../docs/report.md).

Each suite has `*.csv` (one row per measured phase, schema in the top-level README),
`*.meta.json` (environment, config + SHA-256, gate results, failed points) and
`*.graphs.jsonl` (per-graph fanout/fanin histograms). Regenerate a report with
`python3 scripts/analyze_results.py results/<suite>_*.csv --out results/<suite>_report.md`.

## Commit traceability

The CSVs record `git_commit = 43a62d58b247`. The commits were re-authored afterwards
(GitHub e-mail privacy), which changed their hashes but not their content:

| Recorded in results | Published commit | Tree (identical) |
|---|---|---|
| `43a62d5` | `65fc99c` | `1796f26513f53743742533aad146678695ebf855` |

Check: `git rev-parse 65fc99c^{tree}` prints the tree above.

## Environment and caveats

Cloud container: Intel Xeon @ 2.80 GHz, 4 logical CPUs, 16 GB RAM, Ubuntu 24.04,
GCC 13.3.0, Release (`-O3`), threads not pinned, CPU governor not exposed, `perf`
unavailable. The machine is shared, so repetition-level CV is often 20–35 %. Treat
these numbers as a pilot that exercises the full pipeline. They are not definitive
measurements. Re-run on a dedicated machine before drawing performance conclusions (plan §9.3).

## Open observation: graph build cost at 1M nodes

Building either representation of a 1M-node, 4M-edge graph takes 2.4–3.1 s (≈ 600 ns per
edge), while one full arrival pass over it takes ≈ 150 ms. Timing the CSR constructor's
passes separately (in a scratch program, not committed) put 2.1 s of it in the scatter
pass. That pass writes each edge into four ≈ 16–32 MB arrays at random positions.
The counting pass makes random increments into 8 MB arrays and takes 175 ms. The cost
barely changes with `--no-shuffle` and is not explained by first-touch page faults
(≈ 3 µs per 4 KiB page here, ≈ 80 ms total). Requesting transparent huge pages via
`madvise` gave only ≈ 15 %. A TLB / nested-paging effect of this VM is plausible but
unconfirmed: `perf` is unavailable here. Measure on bare metal with hardware counters
before drawing conclusions about build cost (plan §9.3).
