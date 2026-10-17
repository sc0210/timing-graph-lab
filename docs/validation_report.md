# Validation report

Commit `65fc99c` (recorded as `43a62d5` in the CSVs; see `results/README.md`), Ubuntu 24.04, GCC 13.3.0, Intel Xeon @ 2.80 GHz (4 logical CPUs,
16 GB RAM, cloud container). This report covers the correctness work the plan requires
before any performance conclusion (§7).

## 1. Test levels (§7.1)

| Level | Suite | Tests | What it establishes |
|---|---|---|---|
| L1 | `tests/unit` | 21 | T01–T14 of §7.2 plus representation, comparison, I/O and stats details |
| L2 | `tests/property` | 8 | §5.3 generator requirements and §3 invariants over 100 random graphs (4 topologies × 25 seeds, real and integer delays) |
| L3 | `tests/differential` | 5 | All engine variants vs. independent oracles; oracles vs. each other |
| L4 | `tests/regression` | 4 | Pinned generator fingerprints and timing checksums for 16 fixed graphs; bugs found during development |
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

## 7. Benchmark-time verification

Every measured row of the formal suites compares its result with the reference engine
in the same process (§10 step 7). Each suite was preceded by a passing Gate A–C run.

| Suite | Rows | correctness_pass = false | Not bit-identical | max \|err\| |
|---|---|---|---|---|
| baseline | 2,664 | 0 | 0 | 0 |
| incremental | 4,878 | 0 | 0 | 0 |
| parallel | 4,598 | 0 | 0 | 0 |
| scaling | SCALING_ROWS | SCALING_FAIL | SCALING_BIT | SCALING_ERR |

For graphs with at most 20,000 edges, each run also compares the reference engine with the
relaxation oracle (`phase=oracle_check`). No suite had failed or missing configuration points
(`failed_points` in each `.meta.json`).
