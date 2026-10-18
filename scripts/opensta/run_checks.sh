#!/usr/bin/env bash
# Runs the OpenSTA equivalence matrix: ISCAS circuits, generated graphs of every topology
# with integer and real delays, and the harness self-check (a perturbed critical edge must
# be reported). Exit status 0 only if every comparison matches and every self-check fails.
#
#   scripts/opensta/run_checks.sh PATH/TO/sta [BUILD_DIR]      (BUILD_DIR default: build)
set -uo pipefail
STA=${1:?usage: run_checks.sh PATH/TO/sta [BUILD_DIR]}
BUILD=${2:-build}
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
CMP=("$ROOT/scripts/opensta/compare_opensta.py" --sta "$STA" --tgl "$BUILD/timing_graph_sta")
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

fail=0
check() {  # check EXPECTED_EXIT INPUT [extra args]
  local want=$1; shift
  python3 "${CMP[@]}" "$@"
  local got=$?
  if [ "$got" -ne "$want" ]; then echo "  -> expected exit $want, got $got"; fail=1; fi
}

for f in "$ROOT"/benchmarks/iscas/*.bench; do check 0 "$f"; done
# topology nodes delay-dist seed
while read -r topo nodes dist seed; do
  g="$WORK/${topo}_${nodes}_${dist}.el"
  "$BUILD/timing_graph_bench" --topology "$topo" --nodes "$nodes" --layers 50 --fanout 4 --delay-dist "$dist" \
    --seed "$seed" --algorithm timing --representation csr --repetitions 1 --warmup 0 --oracle-max-edges 0 \
    --dump-graph "$g" > /dev/null || { echo "graph generation failed: $topo"; fail=1; continue; }
  check 0 "$g"
done <<'SPECS'
chain 2000 real 4
layered 10000 int 2
reconvergent 10000 real 3
random 5000 int 1
random 20000 real 5
SPECS

echo "--- self-check: a 1 ns error on a critical edge must be detected"
check 1 "$ROOT/benchmarks/iscas/c17.bench" --perturb-edge critical
check 1 "$WORK/layered_10000_int.el" --perturb-edge critical

[ "$fail" -eq 0 ] && echo "OpenSTA equivalence: all checks passed" || echo "OpenSTA equivalence: FAILED"
exit "$fail"
