# Validation report

Commit `65fc99c` (recorded as `43a62d5` in the CSVs; see `results/README.md`), Ubuntu 24.04, GCC 13.3.0, Intel Xeon @ 2.80 GHz (4 logical CPUs,
16 GB RAM, cloud container). This report covers the correctness work the plan requires
before any performance conclusion (§7).

## 1. Test levels (§7.1)

| Level | Suite | Tests | What it establishes |
|---|---|---|---|
| L1 | `tests/unit` | 22 | T01–T14 of §7.2 plus representation, comparison, I/O and stats details |
| L2 | `tests/property` | 8 | §5.3 generator requirements and §3 invariants over 100 random graphs (4 topologies × 25 seeds, real and integer delays) |
| L3 | `tests/differential` | 5 | All engine variants vs. independent oracles; oracles vs. each other |
| L3 | `tests/stress` | 11 | Degenerate graphs through every engine: 200K-fanout and 200K-fanin stars, a 1M-level chain, isolated nodes, duplicate parallel arcs, delays spanning 1e-3 to 1e6, all-zero delays, complete DAG and bipartite graphs, per-node constraints, an incremental edit storm |
| L3 | `tests/trace` + `tests/traces/*.trace` | 4 | Incremental operation traces (edits, updates in both modes, resets, exact expectations) replayed against the reference after every check; 60 random traces; negative tests prove the replayer reports failures |
| L3 | `fuzz/` (`fuzz_*_replay` in CTest, libFuzzer in CI) | 2 targets | Edge-list reader on untrusted input; differential fuzzing of every engine and the incremental timer against the reference |
| L3 | `tests/netlist` | 8 | `.bench` import: hand-computed golden AT/RT/slack for ISCAS-85 c17 and a sequential circuit, delay models, malformed netlists, combinational loops; imported graphs through every engine |
| L3 | OpenSTA harness (`docs/opensta.md`) | 7 inputs + 2 self-checks | Every node's AT, RT and slack against OpenSTA on a reduced-semantics netlist (external, optional CI workflow) |
| L4 | `tests/regression` | 5 | Pinned generator fingerprints and timing checksums for 16 fixed graphs; bugs found during development |
| L5 | `bench_smoke` + `run_suite.py` | — | Benchmarks run only after L1–L4 and sanitizer builds pass; every measured row is re-verified |

### Required test cases (§7.2)

| Case | Test | Result |
|---|---|---|
| T01 empty graph | `T01_empty_graph` | pass: no crash, 0 nodes / 0 edges in every component |
| T02 single node | `T02_single_node_uses_given_initial_values` | pass |
| T03 single path | `T03_single_path_sums_delays` | pass |
| T04 reconvergence | `T04_reconvergence_takes_max_arrival_and_min_required` (incl. the §3.4 example: AT(E) = 8) | pass |
| T05 multiple sources | `T05_multiple_sources_with_distinct_arrivals` (pull and push) | pass |
| T06 multiple sinks | `T06_multiple_sinks_propagate_required_backward` | pass |
| T07 cycle injection | `T07_cycle_injection_is_detected` (Kahn, DFS, both oracles, IncrementalTimer, self-loop validation) | pass |
| T08 zero delays | `T08_zero_delay_edges_are_not_uninitialized` | pass |
| T09 edge update | `T09_single_edge_update_matches_full_recompute` | pass |
| T10 increase / decrease | `T10_increase_and_decrease_both_match`, `T10b_early_stop_halts_when_value_unchanged` | pass |
| T11 overlapping updates | `T11_overlapping_multi_edge_updates` (same edge twice, same path, shared reconvergence) | pass |
| T12 parallel | `T12_parallel_matches_single_thread` (1, 2, 3, 4, 8 threads × spin/block, pool reuse) | pass |
| T13 List vs CSR | `T13_list_and_csr_agree` (arcs, order, Kahn order, AT/RT/slack) | pass |
| T14 random DAG | `T14_random_dag_topological_order_valid_and_consistent` | pass |

## 2. Independent reference (§7.3)

Three oracles share no code with the engine under test (no `kahn_topo_sort`,
`AdjacencyList`, `CsrGraph`, `eval_arrival` or `eval_required`):

1. `path_oracle`: enumerates every source→v and v→sink path (tiny graphs only).
2. `relaxation_oracle`: Bellman–Ford-style fixed point, with its own sink-peeling cycle check.
3. `reference_engine`: O(V+E) full recompute with its own topological sort (deque, edges
   grouped by `std::stable_sort`), so it scales to the 1M-node benchmarks.

`oracles_agree_exhaustively_on_tiny_graphs` checks that all three agree **bit for bit** on
640 cases (4 topologies × 40 seeds × 4 source/sink constraint variants: default, random
per-source arrivals, random per-sink required times, both). The reference engine is
therefore validated before it serves as the oracle for large graphs (§7.3, last paragraph).
It also agrees with the relaxation oracle on 3000-node graphs of every topology.

A fourth, external reference is OpenSTA, which the lab does not control. On a
reduced-semantics netlist its results match the lab's on every node, up to its
single-precision rounding. See [`opensta.md`](opensta.md).

## 3. Numerical criterion (§7.4)

`close(x, y)` ⇔ x = y or |x − y| ≤ ε_abs + ε_rel · max(|x|, |y|), with ε_abs = ε_rel = 1e-9.
This is a starting point for this lab, not an STA accuracy standard. A NaN or ±∞ anywhere
in a result is a failure, because every node of a DAG has finite AT and RT.

Observed: every engine variant (List/CSR, pull/push, Kahn/DFS order, level-parallel with any
thread count or barrier, conservative and early-stop incremental) is **bit-identical** to
the reference engine. This follows from max/min never rounding and all candidates being
the same IEEE sums. Floating-point associativity still matters elsewhere. With real
delays, WNS is not exactly 0 when the required time equals max arrival, because RT is
summed backward and AT forward (`worst_slack_is_zero_when_required_equals_max_arrival`
uses the tolerance for this reason).

## 4. Quality gates (§7.5)

| Gate | Evidence | Status |
|---|---|---|
| A — basic correctness | `ctest` in `build/` (Release): unit, property, differential, regression, bench_smoke | 5/5 pass |
| B — differential | `test_differential`; every benchmark row is compared with the reference engine | pass |
| C — sanitizers | `build-asan` (ASan + UBSan, `-fno-sanitize-recover=all`): 5/5 pass. `build-tsan` (ThreadSanitizer): 5/5 pass | pass |
| D — benchmark eligibility | `correctness_pass` per row; `run_suite.py` aborts on a failed gate; `analyze_results.py` excludes failed rows | see §6 |

## 5. Do the tests detect bugs? (mutation checks)

Each mutation was applied to a scratch copy and the suites rerun:

| Injected defect | Detected by |
|---|---|
| Early-stop incremental never schedules fanout after a change | unit, differential, bench_smoke |
| Arrival ignores the last fanin arc of each node | all 5 suites |
| Worker slices overlap by one node (two threads write the same AT) | ThreadSanitizer: `data race … propagate_arrival_parallel` |

## 6. Bugs found during development (now regression tests)

| Bug | Found by | Regression test |
|---|---|---|
| The relaxation oracle accepted a cycle with no source: all AT stay −∞, so it "converged" immediately | `relaxation_oracle_rejects_cycle` | `relaxation_oracle_rejects_sourceless_cycle` |
| Timing checksum (FNV over raw double bits) gave identical low 32 bits for all equal-size graphs with integer delays | inspecting regression goldens | `timing_checksum_mixes_all_bits` |
| Path oracle summed each path from the source, so it differed from the DP in the last bit with real delays | exhaustive oracle test | oracle now accumulates in DP order; covered by `oracles_agree_exhaustively_on_tiny_graphs` |
| `LevelPool::run` on a graph with no levels returned before its workers had read the job, so a worker could dereference the caller's destroyed `Levels` (ASan stack-use-after-return; segfault in Release with a reused pool). Present since v0.1.0. | `fuzz_engine`, on its first run | `level_pool_empty_graph_with_threads` (fails under ASan without the fix in 3 of 3 runs) |
| The `.bench` importer turned a one-gate combinational loop (`q = NAND(q)`) into a self-loop edge, which `validate()` rejects. That broke the documented contract that loops become cycles detected by the topological sort. | `fuzz_bench_parser` | `parse_bench` rejects it; cases in `parser_rejects_malformed_netlists` (fail without the fix) and `fuzz/corpus/bench_parser/one_gate_loop_crash` |
| `read_edge_list` trusted the edge count in the header and allocated it up front: a 30-byte file declaring 4 billion edges requested ≈ 64 GB | review while adding the reader fuzzer | `parse_edge_list` bounds counts by input size; seed `fuzz/corpus/edge_list_reader/forged_edge_count` |

## 6a. Fuzzing

Two libFuzzer targets (Clang 18, ASan + UBSan, coverage-guided), run locally for 3 minutes each
before the first commit and for 60 s per target in every CI run:

| Target | Inputs executed (local run) | Coverage edges | Findings |
|---|---|---|---|
| `fuzz_edge_list_reader` | 2,078,145 | 355 | none after the header-count fix |
| `fuzz_engine` | 186,182 | 1,124 | the `LevelPool` bug above, found within the first second |
| `fuzz_bench_parser` (with `fuzz/bench_parser.dict`) | 2,103,677 | 735 | a one-gate combinational loop `q = NAND(q)` produced a self-loop edge; fixed (see below) |

The minimized corpora (174, 323 and 435 inputs) are committed in `fuzz/corpus/` and replayed by the
`fuzz_*_replay` CTest tests on every compiler, together with 3,000 pseudo-random engine inputs.

## 7. Benchmark-time verification

Every measured row of the formal suites compares its result with the reference engine
in the same process (§10 step 7). Each suite was preceded by a passing Gate A–C run.

| Suite | Rows | correctness_pass = false | Not bit-identical | max \|err\| |
|---|---|---|---|---|
| baseline | 2,664 | 0 | 0 | 0 |
| incremental | 4,878 | 0 | 0 | 0 |
| parallel | 4,598 | 0 | 0 | 0 |
| scaling | 7,992 | 0 | 0 | 0 |

For graphs with at most 20,000 edges, each run also compares the reference engine with the
relaxation oracle (`phase=oracle_check`). No suite had failed or missing configuration points
(`failed_points` in each `.meta.json`).
