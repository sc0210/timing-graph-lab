# Results

Pilot run on 2026-10-17. Every suite passed Gates A–C (Release, ASan+UBSan and TSan
`ctest`) before measuring. Every measured row was re-verified against the reference engine.

| Suite (plan experiment) | Raw data | Report |
|---|---|---|
| baseline (Exp. B, E; A at 100K) | `baseline_20261017-152536.*` | [baseline_report.md](baseline_report.md) |
| incremental (Exp. C) | `incremental_20261017-152805.*` | [incremental_report.md](incremental_report.md) |
| parallel (Exp. D) | `parallel_20261017-153443.*` | [parallel_report.md](parallel_report.md) |
| scaling (Exp. A1–A4) | `scaling_20261017-154952.*` | [scaling_report.md](scaling_report.md) |

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
