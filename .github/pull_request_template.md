## Summary

<!-- What does this change and why? Link the issue it addresses (e.g. "Closes #12"). -->

## Type of change

- [ ] Bug fix
- [ ] New engine variant / representation / generator
- [ ] Experiment, config or analysis
- [ ] Documentation
- [ ] Build / CI / tooling

## Correctness

- [ ] New or changed engine paths are compared with the independent reference (`include/sta/oracle.hpp`) in `tests/differential` and in the benchmark (`correctness_pass`)
- [ ] A fixed bug has a regression test in `tests/regression`
- [ ] `ctest` passes in the Release, ASan/UBSan and TSan builds
- [ ] Generator output is unchanged, **or** the goldens were regenerated and this is noted in `CHANGELOG.md`

## Performance (if the change affects timing)

<!-- Before/after tables from analyze_results.py on the same machine, the config used,
     and the relevant .meta.json fields (CPU, compiler, build type). Negative results are welcome. -->

## Documentation

- [ ] `README.md` / `docs/experiment_protocol.md` updated for changed CLI options, CSV columns or timing boundaries
- [ ] `CHANGELOG.md` entry added
