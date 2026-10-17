#!/usr/bin/env python3
"""Generate the three report figures of plan §12.1 from timing_graph_bench CSVs.

usage: scripts/plot_results.py results/<suite>_*.csv [...] [--out-dir results/figures] [--only fig1,fig2,fig3]

Each figure should be drawn from the suite designed for it, so that pooled rows of other
suites do not mix into one point:
  plot_results.py results/scaling_*.csv     --only fig1
  plot_results.py results/parallel_*.csv    --only fig2
  plot_results.py results/incremental_*.csv --only fig3

  fig1_runtime_vs_size.png       arrival and build time vs edge count, List vs CSR, per topology
  fig2_parallel_speedup.png      speedup vs threads per topology and size, with the y = x ideal
  fig3_incremental_speedup.png   S_inc = T_full / T_inc vs update ratio per distribution, with 1.0 break-even

Every point is the median over repetitions, then the median over seeds, of rows that
passed verification (Gate D, via analyze_results.summarize). A figure whose data is not
in the given CSVs is skipped with a message. Requires matplotlib (pip install matplotlib).

Colors: the first three slots of the validated reference categorical palette (identity,
fixed order). Every series also gets its own marker and a legend entry, so identity never
depends on color alone; slot 3 is below 3:1 contrast on the surface, so the exact values are
in the analyze_results.py tables (the table view).
"""
import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from analyze_results import load, summarize  # noqa: E402

try:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.ticker import FuncFormatter
except ImportError:
    sys.exit("matplotlib is not installed (pip install matplotlib); analyze_results.py gives the same data as tables")

SURFACE = "#fcfcfb"
TEXT = "#0b0b0b"
TEXT_2 = "#52514e"
MUTED = "#8a8984"
GRID = "#e7e6e2"
SLOTS = ["#2a78d6", "#eb6834", "#1baf7a"]  # categorical slots 1-3, fixed order
MARKERS = ["o", "s", "^"]
TOPOLOGIES = ["chain", "layered", "reconvergent", "random"]

# summarize() key: (topology, nodes, fanout, repr, phase, algorithm, threads, ratio, dist)


def style():
    plt.rcParams.update({
        "figure.facecolor": SURFACE, "axes.facecolor": SURFACE, "savefig.facecolor": SURFACE,
        "axes.edgecolor": GRID, "axes.labelcolor": TEXT_2, "axes.titlecolor": TEXT, "axes.titlesize": 11,
        "axes.titleweight": "bold", "axes.labelsize": 9, "xtick.color": TEXT_2, "ytick.color": TEXT_2,
        "xtick.labelsize": 8, "ytick.labelsize": 8, "axes.grid": True, "grid.color": GRID, "grid.linewidth": 0.6,
        "axes.spines.top": False, "axes.spines.right": False, "lines.linewidth": 2, "lines.markersize": 6,
        "legend.frameon": False, "legend.fontsize": 8, "font.size": 9, "text.color": TEXT,
    })


def series(ax, xs, ys, slot, label, linestyle="-"):
    """Plot one series; markers carry a surface-colored ring so overlapping points stay distinct."""
    ax.plot(xs, ys, color=SLOTS[slot], marker=MARKERS[slot], linestyle=linestyle, label=label,
            markeredgecolor=SURFACE, markeredgewidth=1.5, markersize=7, zorder=3)


def finish(fig, path, title, subtitle):
    fig.suptitle(title, x=0.01, ha="left", fontsize=13, fontweight="bold", color=TEXT)
    fig.text(0.01, 0.945, subtitle, ha="left", va="top", fontsize=9, color=TEXT_2)
    fig.tight_layout(rect=(0, 0, 1, 0.93))
    fig.savefig(path, dpi=160)
    plt.close(fig)
    print(f"wrote {path}")


def topologies_in(S, pred):
    present = {k[0] for k in S if pred(k)}
    return [t for t in TOPOLOGIES if t in present]


def fig_runtime(S, out):
    """Fig. 1: runtime vs graph size, List vs CSR (plan §12.1, Experiment A)."""
    phases = [("arrival", {"list": "pull", "csr": "pull"}, "Arrival propagation"),
              ("build", {"list": "push_back", "csr": "counting_sort"}, "Graph build (both directions)")]
    topos = [t for t in topologies_in(S, lambda k: k[4] == "arrival" and k[5] == "pull") if t != "chain"]
    sizes = {k[1] for k in S if k[4] == "arrival" and k[5] == "pull"}
    if not topos or len(sizes) < 2:
        print("fig1: needs arrival rows at >= 2 graph sizes (run configs/scaling.json); skipped")
        return
    fig, axes = plt.subplots(len(phases), len(topos), figsize=(3.6 * len(topos), 6.4), sharex=True, sharey="row",
                             squeeze=False)
    for r, (phase, algs, label) in enumerate(phases):
        for c, topo in enumerate(topos):
            ax = axes[r][c]
            for slot, repr_ in enumerate(("list", "csr")):
                pts = sorted((v["edges"], v["median"] / 1e3) for k, v in S.items()
                             if k[0] == topo and k[3] == repr_ and k[4] == phase and k[5] == algs[repr_]
                             and k[6] == 1)
                if pts:
                    series(ax, [p[0] for p in pts], [p[1] for p in pts], slot,
                           "Adjacency List" if repr_ == "list" else "CSR")
            ax.set(xscale="log", yscale="log")
            if r == 0:
                ax.set_title(topo)
            if c == 0:
                ax.set_ylabel(f"{label}\nmedian time (ms)")
            if r == len(phases) - 1:
                ax.set_xlabel("edges")
    axes[0][0].legend(loc="upper left")
    finish(fig, out / "fig1_runtime_vs_size.png", "Runtime vs graph size: Adjacency List vs CSR",
           "Median over repetitions and seeds; log-log axes; serial, verified rows only")


def fig_speedup(S, out):
    """Fig. 2: parallel speedup vs threads per topology (plan §12.1, Experiment D)."""
    algs = [("level_parallel_spin", "spin barrier"), ("level_parallel_block", "blocking barrier")]
    rows = sorted({k[1] for k in S if k[5].startswith("level_parallel")})
    topos = topologies_in(S, lambda k: k[5].startswith("level_parallel"))
    if not rows:
        print("fig2: no level_parallel rows (run configs/parallel.json); skipped")
        return
    tmax = max(k[6] for k in S if k[5].startswith("level_parallel"))
    # The chain (one node per level) is orders of magnitude below serial, so it gets its own
    # y scale; all other panels share one range so they can be compared directly.
    fig, axes = plt.subplots(len(rows), len(topos), figsize=(3.3 * len(topos), 3.0 * len(rows) + 0.6),
                             sharex=True, squeeze=False)
    for r, n in enumerate(rows):
        for c, topo in enumerate(topos):
            ax = axes[r][c]
            ax.plot([1, tmax], [1, tmax], color=MUTED, linestyle=(0, (4, 3)), linewidth=1.2, zorder=1)
            ax.axhline(1, color=MUTED, linewidth=0.8, zorder=1)
            base = [S.get((topo, n, f, "csr", ph, "pull", 1, 0.0, "")) for f in {k[2] for k in S if k[0] == topo}
                    for ph in ("arrival", "required")]
            base = [b for b in base if b]
            ax.set_title(f"{topo}, {n // 1000}K nodes" if n < 10**6 else f"{topo}, {n // 10**6}M nodes")
            if len(base) != 2:
                ax.cla()
                ax.axis("off")
                ax.set_title(f"{topo}, {n // 10**6}M nodes" if n >= 10**6 else f"{topo}, {n // 1000}K nodes")
                ax.text(0.5, 0.5, "not run\n(chain capped at 100K nodes)", transform=ax.transAxes, ha="center",
                        va="center", color=MUTED, fontsize=9)
                continue
            else:
                t1 = sum(b["median"] for b in base)
                for slot, (alg, label) in enumerate(algs):
                    ts = sorted({k[6] for k in S if k[0] == topo and k[1] == n and k[5] == alg})
                    pts = []
                    for t in ts:
                        par = [S.get(k) for k in S
                               if k[0] == topo and k[1] == n and k[5] == alg and k[6] == t and k[4] in ("arrival", "required")]
                        if len(par) == 2:
                            pts.append((t, t1 / sum(p["median"] for p in par)))
                    if pts:
                        series(ax, [p[0] for p in pts], [p[1] for p in pts], slot, label)
                d = base[0]["depth"]
                ax.text(0.03, 0.95, f"depth {d:,}", transform=ax.transAxes, va="top", fontsize=8, color=TEXT_2)
            if c == 0:
                ax.set_ylabel("speedup vs serial\n(arrival + required)")
            if r == len(rows) - 1:
                ax.set_xlabel("threads")
            ax.set_xticks(sorted({k[6] for k in S if k[5].startswith("level_parallel")}))
            ax.set_yscale("log", base=2)
            ax.yaxis.set_major_formatter(FuncFormatter(lambda y, _: f"{y:g}×" if y >= 1 else f"1/{1 / y:.0f}×"))
            if topo != "chain":
                ax.set_ylim(0.25, tmax * 1.4)
    axes[0][-1].annotate("ideal y = x", (tmax, tmax), xytext=(-4, -10), textcoords="offset points", ha="right",
                         fontsize=8, color=MUTED)
    # Legend in the bottom-right corner of the first non-chain panel of the last row (empty there).
    for ax in axes[-1]:
        if ax.get_legend_handles_labels()[0] and ax.get_title().split(",")[0] != "chain":
            ax.legend(loc="lower right")
            break
    finish(fig, out / "fig2_parallel_speedup.png", "Level-parallel speedup by topology",
           "T_serial / T_p (algorithm only: pool already running, levels precomputed); dashed = ideal; 1.0 = serial")


def fig_incremental(S, out):
    """Fig. 3: incremental speedup vs update ratio per distribution (plan §12.1, Experiment C)."""
    modes = [("early_stop", "early-stop"), ("conservative", "conservative")]
    dists = ["distributed", "localized", "critical"]
    topos = topologies_in(S, lambda k: k[4] == "incremental")
    if not topos:
        print("fig3: no incremental rows (run configs/incremental.json); skipped")
        return
    n = max(k[1] for k in S if k[4] == "incremental")
    fig, axes = plt.subplots(len(modes), len(topos), figsize=(3.6 * len(topos), 6.4), sharex=True, sharey=True,
                             squeeze=False)
    for r, (mode, mlabel) in enumerate(modes):
        for c, topo in enumerate(topos):
            ax = axes[r][c]
            ax.axhline(1, color=MUTED, linewidth=1.2, linestyle=(0, (4, 3)), zorder=1)
            for slot, dist in enumerate(dists):
                pts = []
                for k, v in S.items():
                    if k[0] == topo and k[1] == n and k[4] == "incremental" and k[5] == mode and k[8] == dist \
                            and k[7] > 0 and v["median"] > 0:
                        full = S.get(k[:4] + ("full_recompute", "pull") + k[6:])
                        if full:
                            pts.append((k[7] * 100, full["median"] / v["median"]))
                pts.sort()
                if pts:
                    series(ax, [p[0] for p in pts], [p[1] for p in pts], slot, dist)
            ax.set(xscale="log", yscale="log")
            ax.xaxis.set_major_formatter(FuncFormatter(lambda x, _: f"{x:g}%"))
            ax.yaxis.set_major_formatter(FuncFormatter(lambda y, _: f"{y:g}×"))
            if r == 0:
                ax.set_title(topo)
            if c == 0:
                ax.set_ylabel(f"{mlabel}\nS_inc = T_full / T_inc")
            if r == len(modes) - 1:
                ax.set_xlabel("edges updated (% of |E|)")
        axes[r][0].annotate("break-even", (axes[r][0].get_xlim()[0], 1), xytext=(4, 4), textcoords="offset points",
                            fontsize=8, color=MUTED)
    axes[-1][0].legend(loc="upper right", title="update distribution", title_fontsize=8)
    finish(fig, out / "fig3_incremental_speedup.png", "Incremental update vs full recompute",
           f"{n:,} nodes; above 1× the incremental update is faster; median over repetitions and seeds")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv", nargs="+")
    ap.add_argument("--out-dir", default="results/figures")
    ap.add_argument("--only", default="fig1,fig2,fig3", help="comma list of figures to draw")
    args = ap.parse_args()
    only = set(args.only.split(","))
    S = summarize(load(args.csv))
    out = Path(args.out_dir)
    out.mkdir(parents=True, exist_ok=True)
    style()
    for name, fn in (("fig1", fig_runtime), ("fig2", fig_speedup), ("fig3", fig_incremental)):
        if name in only:
            fn(S, out)


if __name__ == "__main__":
    main()
