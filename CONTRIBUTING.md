# Contributing to Timing Graph Lab

Thanks for your interest! This project is a research lab, so a contribution counts only
if it keeps the results **correct, attributable and reproducible** (plan §1). This guide
explains how to make changes that meet that bar.

## Ways to contribute

- **Report a bug**, including a wrong timing result, a crash or a flaky test
  ([bug report](https://github.com/sc0210/timing-graph-lab/issues/new?template=bug_report.yml)).
- **Share benchmark results from your machine**. Different CPUs, compilers and core counts
  are valuable ([benchmark result](https://github.com/sc0210/timing-graph-lab/issues/new?template=benchmark_result.yml)).
- **Propose an experiment or feature**, such as a new representation, generator, propagation
  scheme or analysis ([feature request](https://github.com/sc0210/timing-graph-lab/issues/new?template=feature_request.yml)).
- **Improve docs**: the protocol, the validation report or the README.

For larger changes, please open an issue first so the design can be discussed before you
invest time.

## Development setup

Requirements: Linux x86-64, CMake ≥ 3.20, a C++20 compiler (GCC ≥ 13 or Clang ≥ 18 are
tested in CI), Python ≥ 3.10 (standard library only; `matplotlib` optional for plots).

```bash
git clone https://github.com/sc0210/timing-graph-lab && cd timing-graph-lab
source env.sh
tgl_build && tgl_test                                     # Release build + all suites
TGL_BUILD_DIR=build-asan source env.sh && tgl_build Debug && tgl_test
TGL_BUILD_DIR=build-tsan source env.sh && tgl_build RelWithDebInfo && tgl_test
```

## The rules that keep results trustworthy

1. **Correctness before speed.** Every new or optimized engine variant must be compared
   against the independent reference in `include/sta/oracle.hpp`, both in
   `tests/differential` and in the benchmark driver, which writes `correctness_pass`.
   Never compare a variant only against itself or against the engine it replaces.
2. **Oracles stay independent.** `oracle.hpp` must not include or call engine code
   (`kahn_topo_sort`, `AdjacencyList`, `CsrGraph`, `eval_arrival`, `eval_required`).
3. **Fair comparisons.** Variants being compared share the same `EdgeList`, constraints,
   topological order, timing boundary, repetitions and validation (plan §4.4). If a
   variant needs extra preprocessing, time it as its own phase. Never hide it.
4. **Bugs become regression tests.** A fixed bug gets a case in `tests/regression`.
5. **Determinism.** Generators use the project's `Rng` and never `std::` distributions,
   because their output differs between standard libraries. Changing generator output is
   a breaking change: regenerate the goldens (`build/test_regression --print`) and say so
   in the PR and in `CHANGELOG.md`.
6. **Report results honestly.** Slow results, regressions and negative findings (an
   incremental update that is slower than full recompute, a parallel version slower than
   serial) are results. Do not tune configurations to hide them.

## Code style

- C++20, formatted with the repository's `.clang-format` (Google style, 120 columns).
  Run `clang-format -i $(git ls-files '*.cpp' '*.hpp')`; CI checks it.
- No compiler warnings with `-Wall -Wextra -Wpedantic` on GCC and Clang.
- Headers carry a short comment explaining *why* the component exists and its invariants.
  Match the comment density of the surrounding code.
- Python scripts use only the standard library (plotting excepted).

## Pull requests

1. Branch from `main`, keep the PR focused on one change, and fill in the PR template.
2. CI must pass: GCC + Clang builds and tests, ASan/UBSan, TSan, format check, and the smoke suite.
3. If the change affects performance, attach before/after `analyze_results.py` tables
   from the **same machine**, the config used, and the `.meta.json` environment.
4. Update the docs (`README.md`, `docs/experiment_protocol.md`) when you change a CLI
   option, a CSV column or a timing boundary, and add a `CHANGELOG.md` entry.

## Contributing benchmark data

Run a suite from a clean checkout (`git status` must not report modified files). Commits
tagged `-dirty` are not accepted as results. Then open a *benchmark result* issue with:

- the generated report (`analyze_results.py ... --out report.md`),
- the `.meta.json` file (it records CPU, RAM, compiler, flags and gate results),
- anything unusual about the machine (shared host, frequency scaling, NUMA, pinning).

Raw CSVs can be attached as a zip or linked. Results from different CPU architectures are
analyzed separately, never merged.

## Code of conduct

This project follows the [Code of Conduct](CODE_OF_CONDUCT.md). By participating you agree
to uphold it.
