# Timing Graph Lab — Pilot Experiment Report

Version 1.0, 2026-10-17. Each experiment follows the plan's §12.2 template. Every number
comes from the committed CSVs in [`results/`](../results/README.md), via the per-suite reports
(`results/<suite>_report.md`) and figures (`results/figures/`).

**How to read this report.** Each section separates four kinds of statement:

- **Measured**: a value read from the CSVs (median over 10 repetitions after 2 warm-ups,
  then the median over 3 seeds unless stated).
- **Hypothesis**: what we expected and why. The hypotheses were *not* registered before
  these pilot runs. They are the expectations the plan implies, written down afterwards,
  and are labelled as such.
- **Interpretation**: a possible explanation. Unless an "evidence" line says otherwise, it
  is unverified.
- **Limitation**: what was not controlled or verified.

## Common setup

| Item | Value |
|---|---|
| Code | commit `65fc99c` (recorded as `43a62d5` in the CSVs; identical tree, see `results/README.md`) |
| Machine | cloud VM, Intel Xeon @ 2.80 GHz, 4 logical CPUs, 16 GB RAM, kernel 6.18 (Firecracker) |
| Toolchain | Ubuntu 24.04, GCC 13.3.0, CMake Release (`-O3`), no `-march=native` |
| Graphs | §5.2 baseline: fanout target 4, 100 layers (layered / reconvergent), seeds 20261017–20261019, real delays uniform in [1, 100), shuffled node ids |
| Protocol | [`docs/experiment_protocol.md`](experiment_protocol.md): timing boundaries, statistics, reset rules |
| Gates | each suite ran only after Release, ASan+UBSan and TSan `ctest` passed (`gates` in every `.meta.json`) |
| Not available | `perf` (no hardware counters), CPU pinning, frequency governor control |

**Global limitation.** The VM is shared, and the coefficient of variation across
repetitions is often 20–35 % (up to ~115 % seed spread at 10K nodes, where phases take
< 1 ms). Differences below about 1.3× should not be treated as established. All results
are a pilot on one machine.

## Correctness summary (applies to every experiment)

**Measured:** 20,132 measured rows across the four suites, **0** with
`correctness_pass = false`, **0** not bit-identical to the reference engine, max |error| 0.
The reference engine was itself checked bit for bit against path enumeration and a
relaxation oracle on 640 small cases. See [`validation_report.md`](validation_report.md).

---

## Experiment A — Adjacency List vs CSR (RQ1, RQ6)

**1. Research question.** Which representation suits a timing graph better in build
time, propagation time and memory, and does the answer change with size?

**2. Hypothesis (post hoc).** CSR propagates faster and uses less memory because each
direction is three contiguous arrays instead of one heap vector per node. Its build may
cost more, and structural edits will be much more expensive (full rebuild).

**3. Setup.** `configs/scaling.json`: layered, reconvergent and random DAGs with 10K /
100K / 500K / 1M nodes (≈ 40K–4M edges), 3 seeds, both representations,
`--algorithm traversal,timing,structure`.

**4. Measurements** (scaling suite, [report](../results/scaling_report.md), [Fig. 1](../results/figures/fig1_runtime_vs_size.png)):

| Metric (List ÷ CSR) | 10K nodes | 100K nodes | 1M nodes |
|---|---|---|---|
| Structure memory | 1.72–1.90× | 1.72–1.90× | 1.73–1.90× |
| Build, both directions | 2.4–3.9× | 1.76–1.93× | 1.21–1.26× |
| Arrival, pull | 1.2–2.8× (noisy) | 2.07–2.31× | 1.06–1.24× |
| Required, pull | 1.08–1.20× (noisy) | 2.25–2.51× | 1.16–1.20× |
| Arrival, push | 1.07–1.63× | 2.98–4.55× | 1.34–1.51× |

Absolute values at 1M nodes: CSR arrival 133–151 ms, build 2.4–2.5 s, structure 106 MiB
(List: 183–203 MiB). Process peak RSS is 653–706 MiB at 1M nodes; this covers both
representations plus the reference engine in the same process.

Structural edit, inserting 1 % new edges (`edge_insert`): List append is 1.9–3.1 ms vs
CSR rebuild 102–106 ms at 100K nodes, and 36–59 ms vs 2.4–2.5 s at 1M nodes. CSR is
≈ 33–66× slower.

**5. Correctness.** All rows pass. Both representations produce the same Kahn order
(`same_order_as_first_repr=1`) and bit-identical AT, RT and slack.

**6. Observations.**
- CSR is faster and smaller at every size. Its build is *also* faster, so there is no
  break-even point: the CSR is cheaper from the first pass.
- CSR's propagation advantage peaks at 100K nodes (≈ 2–2.5×) and shrinks to 1.06–1.3×
  at 1M nodes.
- Push-style arrival (the plan's §6.2 pseudo-code) is ≈ 2× (CSR) to ≈ 4× (List) slower
  than pull on the same representation at 100K nodes.
- Structural edits strongly favor the Adjacency List.

**7. Interpretation.**
- *Possible:* at 100K nodes, CSR arcs (10.6 MiB) are close to cache size while the
  List's scattered per-node vectors are not. At 1M nodes, both are dominated by random
  gathers of `AT[pred]` over a 1M-entry array, which is identical for both.
- *Possible:* push is slower because each arc does a read-modify-write to a random
  successor, whereas pull writes each node once.
- *Evidence:* none beyond runtime. **No cache or TLB counters were collected, so cache
  locality is not established as the cause** (plan §12.2 example).
- The 1M-node build cost (≈ 600 ns per edge, mostly the random scatter pass) is an open
  observation; see `results/README.md`.

**8. Limitations.** One machine, shared VM, no counters. Node ids are shuffled. With
locality-preserving ids (`--no-shuffle`), the picture may change and was not measured
systematically. Only the 16-byte `{node, delay}` arc layout was tested for the List.

**9. Next experiment.** Re-run on bare metal with `perf stat` (cache-misses, dTLB-misses).
Add a `--no-shuffle` axis to separate representation effects from id locality.

---

## Experiment B — Graph topology (RQ2)

**1. Research question.** How do topology and connectivity affect propagation efficiency?

**2. Hypothesis (post hoc).** At equal node count, per-edge cost is similar across DAG
shapes. The chain has 4× fewer edges, so it is faster in total but not per edge.
Topology matters more for parallelism (Experiment D) than for serial cost.

**3. Setup.** `configs/baseline.json`: four topologies at 100K nodes, fanout target 4,
3 seeds, both representations.

**4. Measurements** (baseline suite, CSR, serial pull arrival, [report](../results/baseline_report.md)):

| Topology | Edges | Sources / sinks | Max fanout / fanin | Depth | Median level width | Arrival | ns / edge |
|---|---|---|---|---|---|---|---|
| chain | 99,999 | 1 / 1 | 1 / 1 | 100,000 | 1 | 1.12 ms | 11.2 |
| layered | 396,000 | 1,000 / 2,866 | 14 / 4 | 100 | 1,000 | 2.81 ms | 7.1 |
| reconvergent | 396,000 | 1,000 / 2,981 | 17 / 4 | 100 | 1,000 | 3.42 ms | 8.6 |
| random | 400,000 | 1,799 / 20,055 | 46 / 16 | 82 | 335 | 3.31 ms | 8.3 |

**5. Correctness.** All rows pass.

**6. Observations.** Per-edge cost differs by at most ≈ 1.6× across topologies. The chain
has the highest cost *per edge* but the lowest total. Shapes differ strongly in depth
(82 vs 100,000) and level width (1 vs 1,000).

**7. Interpretation.** *Possible:* the chain pays a per-node overhead (one fanin span
lookup per edge) that the other graphs amortize over ~4 fanin arcs per node. The
reconvergent and random graphs draw fanin from farther away (several layers back, or
anywhere), which may cost locality relative to the layered graph. No counters were
collected to confirm this.

**8. Limitations.** The edge counts are not equal: the chain cannot have fanout 4. Run-to-run
CV is about 30 %, so the layered vs reconvergent vs random differences (≈ 20 %) are
within noise.

**9. Next experiment.** Compare at equal edge count, and sweep fanout (2, 4, 8) at fixed
topology.

---

## Experiment C — Incremental STA vs full recompute (RQ4)

**1. Research question.** Is a local timing update faster than full recomputation, and
for which update sizes and distributions?

**2. Hypothesis (post hoc).** Incremental update wins for small update sets and loses
as the update ratio grows. Localized updates benefit most and updates near critical
paths least. Early stop beats the conservative region, because the region over-
approximates the nodes that actually change.

**3. Setup.** `configs/incremental.json`: layered, reconvergent and random DAGs at 100K
nodes, 3 seeds, CSR, update ratios 0 / 0.01 / 0.1 / 1 / 5 / 10 % (0.01 % and 0.1 % added
beyond the plan's §8C ratios), distributions localized / distributed / critical, modes
conservative / early-stop. Every repetition is reset to the original delays.

**4. Measurements** ([report](../results/incremental_report.md), [Fig. 3](../results/figures/fig3_incremental_speedup.png); S_inc = T_full / T_inc, above 1 means the incremental update is faster):

| Mode, distribution | 0.01 % (≈ 40 edges) | 0.1 % | 1 % | 10 % |
|---|---|---|---|---|
| early-stop, distributed | 20–78× | 1.2–3.2× | 0.14–0.28× | 0.09–0.12× |
| early-stop, localized | 11–36× | 2.1–6.3× | 0.31–0.45× | 0.13–0.16× |
| early-stop, critical | 0.19–0.27× | 0.14–0.21× | 0.14–0.16× | 0.09–0.13× |
| conservative, any | 0.23–0.62× | 0.24–0.66× | 0.21–0.40× | 0.14–0.25× |

Work at 0.01 %, distributed, layered: affected region 99,810 nodes (99.8 % of the graph),
early-stop evaluated 556 nodes, and 119 nodes actually changed. Full recompute takes
8.9–14.5 ms.

**5. Correctness.** Every incremental result (both modes, every ratio and distribution,
every repetition) matches the reference engine on the modified graph bit for bit, and
matches the full recompute.

**6. Observations.**
- The conservative region (fanout cone ∪ fanin cone) covers 72–100 % of the graph even
  for 40 edited edges, so the conservative mode never wins.
- Early stop wins strongly for 0.01 % non-critical edits, breaks even around 0.1 %, and
  loses from 1 % upward.
- Edits concentrated on low-slack edges change most of the graph's timing. At 0.01 %,
  82–98 % of nodes change, so even early stop loses.

**7. Interpretation.**
- With fanout ≈ 4 and depth 100, a single edge's fanout cone quickly spans the graph. The
  reachable region is therefore a poor proxy for the work needed, and change detection
  is what makes incremental STA pay off. This follows directly from the measured
  affected / evaluated / changed counts.
- Critical-biased edits change most nodes because the required time is set to the
  maximum arrival. Every node's slack therefore depends on the critical paths, and
  editing them shifts RT everywhere.
- *Possible:* per evaluated node, the incremental update costs several times a full pass
  (priority-queue operations, random order). That would explain why it loses even when
  it evaluates fewer nodes than a full pass (e.g. 1 %, distributed). Not profiled.

**8. Limitations.** Delay-only edits; topology fixed (no cell insertion or
restructuring; see plan §13). One fanout setting. The RT constraint (period = max
arrival) maximizes critical-path coupling; a looser period would reduce RT changes.
New delays are drawn independently of the old ones, so a 1 % edit is a large
perturbation.

**9. Next experiment.** Replace the binary heap with level-bucket queues. Use small
relative delay perturbations (± 5 %). Sweep the period, and run single-edge ECO loops.

---

## Experiment D — Level-parallel propagation (RQ5)

**1. Research question.** Does multithreading speed up timing propagation, and what limits it?

**2. Hypothesis (post hoc).** Speedup grows with level width and graph size and is
capped by per-level barrier cost. The chain (width 1) gets no speedup.

**3. Setup.** `configs/parallel.json`: CSR, 1 / 2 / 4 threads, spin vs blocking barrier,
layered / reconvergent / random DAGs at 100K and 1M nodes (3 seeds), chain at 100K
(1 seed). T₁ is the serial pull arrival + required time; algorithm-only timing (pool
running, levels precomputed).

**4. Measurements** ([report](../results/parallel_report.md), [Fig. 2](../results/figures/fig2_parallel_speedup.png)):

| Graph | Spin, 2 threads | Spin, 4 threads | Blocking, 4 threads | End-to-end, spin, 4 threads |
|---|---|---|---|---|
| layered / reconvergent / random, 1M | 1.48–1.68× | **2.47–2.56×** (efficiency 62–64 %) | 2.01–2.30× | 0.86–0.98× |
| layered / reconvergent / random, 100K | 1.15–1.48× | 1.18–2.15× | 0.54–0.82× | 0.60–0.81× |
| chain, 100K | 0.04× | 0.016× | 0.0003× | 0.016× |

Per-level cost in the chain at 4 threads: ≈ 1.9 µs (spin) vs ≈ 94 µs (blocking); at 2
threads ≈ 0.7 µs vs ≈ 4.2 µs. Levelization plus thread start-up costs about as much as one parallel pass
(end-to-end column).

**5. Correctness.** All parallel results are bit-identical to the reference for every
thread count and barrier. TSan reports no race, and it does detect a deliberately
injected one (validation report §5).

**6. Observations.**
- Useful speedup appears only for wide levels (10,000 nodes per level at 1M). Efficiency
  stays well below ideal even there.
- The blocking barrier makes 100K-node graphs slower than serial.
- The chain is 23–63× (spin) and up to ≈ 3,000× (blocking, 4 threads) slower than serial.
  This is kept as a result, as plan §6.6 requires.
- End-to-end, including levelization and pool start-up, no configuration beats serial
  for a single pass.

**7. Interpretation.**
- The barrier dominates whenever the work per level is small. The chain isolates the
  barrier cost: ≈ 94 µs per blocking barrier at 4 threads. Two passes × 100 levels × 94 µs
  ≈ 19 ms, more than a whole serial arrival + required pass on a 100K-node layered graph
  (≈ 9–10 ms). This matches the blocking barrier being slower than serial at 100K.
- *Possible:* the 4 vCPUs share physical cores and memory bandwidth (VM), which caps
  efficiency at 1M nodes. Unverified.
- Level parallelism pays off only when levels are reused across many passes (repeated
  timing queries on a fixed topology).

**8. Limitations.** 4 logical CPUs only; threads not pinned; spin barrier assumes
dedicated cores. Static partitioning only.

**9. Next experiment.** Bare metal with 8–32 cores and pinning. Amortize levelization
over many passes. Try dynamic or work-stealing scheduling and task-graph propagation
(plan §6.6, after stable correctness).

---

## Experiment E — BFS / DFS / Kahn (RQ3)

**1. Research question.** What are the roles and costs of BFS, DFS and Kahn's
topological sort in STA?

**2. Hypothesis (post hoc).** All three are O(V + E) traversals of similar cost. Only
Kahn and DFS give a valid propagation order; BFS order fails on graphs with
reconvergence across different depths.

**3. Setup.** Baseline suite (100K nodes, four topologies, CSR). Traversal cost is measured
separately from the same pull propagation run over the Kahn order vs the DFS order.

**4. Measurements** ([baseline report](../results/baseline_report.md)):

| Topology | Kahn (indegree init) | DFS | BFS | BFS order valid? | Arrival over Kahn order / DFS order |
|---|---|---|---|---|---|
| chain | 5.18 ms (0.20) | 4.19 ms | 3.19 ms | yes | 1.12 / 1.82 ms |
| layered | 6.02 ms (0.21) | 5.89 ms | 4.17 ms | yes | 2.81 / 4.85 ms |
| reconvergent | 6.55 ms (0.21) | 6.92 ms | 4.93 ms | **no** | 3.42 / 5.08 ms |
| random | 7.05 ms (0.20) | 6.72 ms | 4.54 ms | **no** | 3.31 / 5.04 ms |

**5. Correctness.** Kahn and DFS orders are valid on every graph (checked edge by edge).
Arrival over either order matches the reference exactly. A BFS order that is not
topological is never used for propagation.

**6. Observations.** The three traversals cost within ≈ 1.6× of each other. BFS order is
invalid exactly on the graphs where paths of different hop length reconverge, as
predicted in plan §3.4. Propagating over the DFS order is 1.5–1.7× slower than over the
Kahn order.

**7. Interpretation.** *Possible:* the Kahn order visits nodes roughly level by level, so
consecutive nodes have fanin in recently touched levels. The DFS order jumps between
deep branches. This is consistent with the timings but not confirmed with counters.

**8. Limitations.** DFS is iterative with an explicit stack; recursion was not tested.
BFS is measured only as a traversal, not as reachability or cone extraction for
incremental STA, which is its real use in an STA flow.

**9. Next experiment.** Measure BFS and DFS in their STA roles (affected-cone extraction
for incremental updates, cycle detection on inputs). Try a level-sorted order (sort
nodes by level, then by id) as a locality-optimized propagation order.

---

## Overall conclusions (pilot, this machine)

1. A **CSR with both directions** is the better default for static timing propagation:
   smaller, faster to build and faster to propagate. Graphs that change structurally favor
   the Adjacency List by about 33–66× per edit batch.
2. **Pull-based propagation over a Kahn order** is the fastest serial scheme measured.
3. **Incremental STA needs change detection.** Region-based recomputation does not pay
   off on fanout-4 graphs. Event-driven early stop pays off only for very small,
   non-critical edit sets (≲ 0.1 % of edges).
4. **Level-parallel propagation** needs wide levels, cheap barriers and reuse of the
   level structure. Deep, narrow graphs should stay serial.
5. **Correctness held throughout:** every optimized variant is bit-identical to an
   independent reference, which is what makes these comparisons meaningful.

The most valuable next step is the same suites on a dedicated multi-core machine with
hardware counters. That would turn the "possible" interpretations above into tested ones.
Before that re-run, the hypotheses should be registered in `docs/hypotheses.md`.
