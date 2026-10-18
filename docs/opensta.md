# OpenSTA equivalence check

[OpenSTA](https://github.com/The-OpenROAD-Project/OpenSTA) is a production static timing
analyzer. It serves here as an **external, independent reference**, alongside the three
internal oracles of [`validation_report.md`](validation_report.md). OpenSTA is GPL-3.0 and
runs as a separate program; it is never vendored or linked into this MIT-licensed
repository.

## What is compared, and why it needs a reduced netlist

OpenSTA implements full STA semantics: Liberty table delays depending on slew and load,
separate rise and fall transitions with unateness, slew propagation, parasitics, clocks,
setup/hold checks. The lab's phase-1 model is a DAG with one fixed delay per edge and a
single max-plus / min-plus pass. The harness therefore writes OpenSTA inputs in which
OpenSTA's semantics **reduce exactly to the lab's model**:

| Lab | OpenSTA collateral |
|---|---|
| node with fanin | instance `u<v>` of cell `TGL_K<k>`: k inputs, positive-unate OR, zero cell delay, zero transition |
| node without fanin | input port `n<v>` |
| sink (fanin, no fanout) | output port `n<v>`, driven by `u<v>/Z` |
| edge u → v, delay d | net from u's driver to an input pin of `u<v>`; `d` annotated as SDF `INTERCONNECT` |
| a0 = 0, common r0 | virtual clock with period r0; input delay 0 on all inputs, output delay 0 on all outputs |

With zero transitions and identical rise/fall behavior, OpenSTA's max arrival and max
(setup) required time on each pin equal the lab's AT and RT. The harness reads them back
with `report_arrival`, `report_required` and the pin property `slack_max`. Isolated nodes
have no OpenSTA counterpart and are skipped.

## Precision: why the comparison has two stages

OpenSTA computes in **single precision with times in seconds**. An SDF value `d` becomes
`float32(float32(d) * float32(1e-9))` (`sdf/SdfReader.cc`), and reports convert back by float
division. The lab computes in double precision, in nanoseconds. Compared directly, the two
differ by an amount that grows with path length: 0.17 ns on a 2,000-deep chain at
≈ 99,000 ns. That needs a tolerance loose enough to hide real errors.

`compare_opensta.py` therefore compares both against an **independent Python
implementation of the lab's model** (Kahn order, max-plus arrival, min-plus required, same
association order as the engine):

| Check | Compared | Tolerance | Measured |
|---|---|---|---|
| A | `timing_graph_sta` vs model in double | 1e-9 + 1e-12 relative | **0** on every input (bit-identical) |
| B | OpenSTA vs model in float32, using OpenSTA's conversions | 8 float32 ulps of the graph's largest time + 1e-6 | arrival: equal to the printed digits; required: ≤ 2 ulps |

The residual of up to 2 ulps on required times comes from an operation order inside
OpenSTA's required-time computation that the model does not reproduce. The 8-ulp tolerance
is ≈ 0.004 ns at a 4,445 ns time scale and ≈ 0.1 ns at 99,000 ns.

**The harness can fail.** `--perturb-edge critical` adds 1 ns to an edge on a zero-slack path,
in the OpenSTA collateral only. In every run of the matrix this produced mismatches (exit 1).
An earlier version of the harness accidentally applied the perturbation to the reference
model too, and the self-check exposed it by passing.

## Results

OpenSTA commit `0f2529d` (2026-10-16), CUDD `f54f533`, lab commit on branch
`feature/opensta-harness`; `scripts/opensta/run_checks.sh`:

| Input | Nodes | Edges | Time scale | max \|engine − model\| | max \|OpenSTA − float32 model\| | raw \|engine − OpenSTA\| | Result |
|---|---|---|---|---|---|---|---|
| ISCAS-85 c17 | 13 | 14 | 3 ns | 0 | 4.8e-7 | 2.4e-7 | match |
| `seq_example.bench` (2 DFFs) | 12 | 11 | 3 ns | 0 | 4.8e-7 | 2.4e-7 | match |
| chain, real delays | 2,000 | 1,999 | 99,461 ns | 0 | 4.5e-7 | 0.17 | match |
| layered, integer delays | 9,994 | 39,200 | 4,445 ns | 0 | 9.8e-4 | 1.6e-3 | match |
| reconvergent, real delays | 10,000 | 39,200 | 3,809 ns | 0 | 4.9e-4 | 1.4e-3 | match |
| random, integer delays | 4,979 | 20,000 | 3,034 ns | 0 | 4.9e-4 | 7.3e-4 | match |
| random, real delays | 19,944 | 80,000 | 3,988 ns | 0 | 5.0e-7 | 1.6e-3 | match |
| self-check, c17 + 1 ns | | | | 0 | 1 | 1 | **mismatch, as required** |
| self-check, layered + 1 ns | | | | 0 | 1 | 1 | **mismatch, as required** |

**Conclusion.** On these inputs, OpenSTA and the lab compute the same timing, up to
OpenSTA's single-precision rounding. The reduced-semantics mapping is correct. This does
not validate anything outside the lab's model: slew, rise/fall asymmetry, unateness and
clocks are deliberately neutralized.

## Running it

```bash
# 1. Build OpenSTA (needs Tcl, SWIG, Bison, Flex, Eigen, zlib; CUDD built from source)
git clone https://github.com/ivmai/cudd.git && (cd cudd && git checkout f54f533 && autoreconf -fi \
  && ./configure --prefix=$PWD/install && make -j && make install)
git clone https://github.com/The-OpenROAD-Project/OpenSTA.git && (cd OpenSTA && git checkout 0f2529d \
  && cmake -B build -DCUDD_DIR=$PWD/../cudd/install -DBUILD_TESTS=OFF -DUSE_TCL_READLINE=OFF && cmake --build build -j)

# 2. One input, or the whole matrix
scripts/opensta/compare_opensta.py benchmarks/iscas/c17.bench --sta OpenSTA/build/sta
scripts/opensta/run_checks.sh OpenSTA/build/sta build
```

CI runs the matrix in `.github/workflows/opensta.yml` on pull requests that touch the engine
or the harness, weekly, and on demand. The OpenSTA build is cached per pinned commit.

## Next steps

- Per-source arrival and per-sink required times: input and output delays per port. The
  current harness requires a common r0, because `timing_graph_sta` has no per-node constraints.
- Larger real circuits (ISCAS-85/89, EPFL) once a fetch script with verified sources exists;
  the `.bench` importer already handles them.
- Phase 2: with rise/fall delays and slew in the lab's model, the same harness, without the
  neutralizing Liberty, becomes a much stronger oracle.
