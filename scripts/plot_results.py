#!/usr/bin/env python3
"""Plot results from timing_graph_bench CSVs (requires matplotlib: pip install matplotlib).

usage: scripts/plot_results.py results/<suite>_*.csv [--out-dir results/plots]
  expA_scaling.png      arrival time vs edge count, list vs CSR (Experiment A)
  expC_incremental.png  S_inc vs update ratio per distribution (Experiment C)
  expD_speedup.png      speedup vs threads per topology/barrier (Experiment D)
Only rows that passed verification are plotted (Gate D).
"""
import argparse
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from analyze_results import load, summarize  # noqa: E402

try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
except ImportError:
    sys.exit("matplotlib is not installed (pip install matplotlib); analyze_results.py gives the same data as tables")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv", nargs="+")
    ap.add_argument("--out-dir", default="results/plots")
    args = ap.parse_args()
    S = summarize(load(args.csv))
    out = Path(args.out_dir)
    out.mkdir(parents=True, exist_ok=True)

    # key = (topology, nodes, fanout, repr, phase, algorithm, threads, ratio, dist)
    fig, ax = plt.subplots(figsize=(7, 4.5))
    series = defaultdict(list)
    for k, v in S.items():
        if k[4] == "arrival" and k[5] == "pull":
            series[(k[0], k[3])].append((v["edges"], v["median"]))
    for (topo, repr_), pts in sorted(series.items()):
        pts.sort()
        ax.plot(*zip(*pts), marker="o", linestyle="-" if repr_ == "csr" else "--", label=f"{topo} / {repr_}")
    ax.set(xscale="log", yscale="log", xlabel="edges", ylabel="arrival, median µs", title="Experiment A: arrival scaling")
    ax.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(out / "expA_scaling.png", dpi=150)

    fig, ax = plt.subplots(figsize=(7, 4.5))
    curves = defaultdict(list)
    for k, v in S.items():
        if k[4] == "incremental" and k[7] > 0:
            full = S.get(k[:4] + ("full_recompute", "pull") + k[6:])
            if full and v["median"] > 0:
                curves[(k[0], k[1], k[8], k[5])].append((k[7] * 100, full["median"] / v["median"]))
    for (topo, n, dist, mode), pts in sorted(curves.items()):
        pts.sort()
        ax.plot(*zip(*pts), marker="o", linestyle="-" if mode == "early_stop" else ":", label=f"{topo} n={n} {dist} {mode}")
    ax.axhline(1, color="gray", linewidth=0.8)
    ax.set(yscale="log", xlabel="update ratio (%)", ylabel="S_inc = T_full / T_inc", title="Experiment C: incremental speedup")
    ax.legend(fontsize=6)
    fig.tight_layout()
    fig.savefig(out / "expC_incremental.png", dpi=150)

    fig, ax = plt.subplots(figsize=(7, 4.5))
    sp = defaultdict(dict)
    for k, v in S.items():
        if k[3] == "csr" and k[5].startswith("level_parallel") and k[4] in ("arrival", "required"):
            d = sp[(k[0], k[1], k[5])]
            d[k[6]] = d.get(k[6], 0) + v["median"]
    tmax = 1
    for (topo, n, alg), byt in sorted(sp.items()):
        ser = [S.get((topo, n, k2, "csr", ph, "pull", 1, 0.0, "")) for k2 in {k[2] for k in S if k[0] == topo and k[1] == n}
               for ph in ("arrival", "required")]
        ser = [x for x in ser if x]
        if len(ser) != 2:
            continue
        t1 = sum(x["median"] for x in ser)
        ts = sorted(byt)
        tmax = max(tmax, ts[-1])
        ax.plot(ts, [t1 / byt[t] for t in ts], marker="o", label=f"{topo} n={n} {alg.replace('level_parallel_', '')}")
    ax.plot([1, tmax], [1, tmax], color="gray", linestyle=":", label="ideal")
    ax.set(xlabel="threads", ylabel="speedup vs serial", title="Experiment D: level-parallel speedup")
    ax.legend(fontsize=7)
    fig.tight_layout()
    fig.savefig(out / "expD_speedup.png", dpi=150)
    print(f"wrote plots to {out}")


if __name__ == "__main__":
    main()
