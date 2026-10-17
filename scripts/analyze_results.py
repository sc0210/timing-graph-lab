#!/usr/bin/env python3
"""Summarize timing_graph_bench CSVs into per-experiment tables (stdlib only).

usage: scripts/analyze_results.py results/<suite>_*.csv [--out report.md] [--summary summary.csv]

Gate D (plan §7.5): rows with correctness_pass=false are excluded from every performance
table and listed separately. Statistics over repetitions: median (primary), p90, min, max
and coefficient of variation (CV > 10% is flagged as noisy). Seeds are aggregated by
taking the median over seeds of the per-seed medians, and the seed spread is reported.
"""
import argparse
import csv
import statistics as st
import sys
from collections import defaultdict

INT_COLS = ("node_count", "edge_count", "thread_count", "rep", "updated_edges", "affected_nodes", "changed_nodes",
            "work", "struct_mem_bytes", "peak_rss_bytes", "depth", "sources", "sinks", "max_fanout", "max_fanin",
            "level_width_median", "level_width_max")


def load(paths):
    rows = []
    for p in paths:
        with open(p, newline="") as f:
            rows += list(csv.DictReader(f))
    for r in rows:
        for k in INT_COLS:
            r[k] = int(r[k])
        r["runtime_us"] = float(r["runtime_us"])
        r["update_ratio"] = float(r["update_ratio"])
        r["pass"] = r["correctness_pass"] == "true"
    return rows


def pct(xs, q):
    xs = sorted(xs)
    return xs[min(len(xs) - 1, int(round(q * (len(xs) - 1))))]


def stats(xs):
    m = st.mean(xs)
    return {"n": len(xs), "median": st.median(xs), "p90": pct(xs, 0.9), "min": min(xs), "max": max(xs),
            "cv": st.pstdev(xs) / m if len(xs) > 1 and m > 0 else 0.0}


# A configuration = everything except seed / rep; a "cell" additionally includes the seed.
CFG = ("topology", "node_count", "fanout_target", "representation", "phase", "algorithm", "thread_count",
       "update_ratio", "update_dist")


def summarize(rows):
    cells = defaultdict(list)
    for r in rows:
        if r["pass"]:
            cells[tuple(r[k] for k in CFG) + (r["seed"],)].append(r)
    per_cfg = defaultdict(list)
    for key, rs in cells.items():
        s = stats([r["runtime_us"] for r in rs])
        s.update(edges=st.median(r["edge_count"] for r in rs), work=st.median(r["work"] for r in rs),
                 mem=max(r["struct_mem_bytes"] for r in rs), affected=st.median(r["affected_nodes"] for r in rs),
                 changed=st.median(r["changed_nodes"] for r in rs), updated=rs[0]["updated_edges"],
                 depth=rs[0]["depth"], lw_median=rs[0]["level_width_median"], sources=rs[0]["sources"],
                 sinks=rs[0]["sinks"], max_fanout=rs[0]["max_fanout"], max_fanin=rs[0]["max_fanin"],
                 notes=rs[0]["notes"])
        per_cfg[key[:-1]].append(s)
    out = {}
    for key, ss in per_cfg.items():
        meds = [s["median"] for s in ss]
        agg = dict(ss[0])
        agg.update(median=st.median(meds), p90=st.median(s["p90"] for s in ss), min=min(s["min"] for s in ss),
                   max=max(s["max"] for s in ss), cv=max(s["cv"] for s in ss), seeds=len(ss),
                   seed_spread=(max(meds) - min(meds)) / st.median(meds) if len(ss) > 1 and st.median(meds) > 0 else 0.0,
                   edges=st.median(s["edges"] for s in ss), work=st.median(s["work"] for s in ss),
                   affected=st.median(s["affected"] for s in ss), changed=st.median(s["changed"] for s in ss),
                   n=sum(s["n"] for s in ss))
        out[key] = agg
    return out


def get(S, topo, n, f, repr_, phase, alg, t=1, ratio=0.0, dist=""):
    return S.get((topo, n, f, repr_, phase, alg, t, ratio, dist))


def us(x):
    return f"{x:,.1f}" if x >= 100 else f"{x:.2f}"


def table(headers, body):
    if not body:
        return "_(no data)_"
    lines = ["| " + " | ".join(headers) + " |", "|" + "|".join("---" for _ in headers) + "|"]
    lines += ["| " + " | ".join(str(c) for c in row) + " |" for row in body]
    return "\n".join(lines)


def graphs(S):
    return sorted({(k[0], k[1], k[2]) for k in S}, key=lambda g: (g[0], g[1], g[2]))


def report(rows, S):
    md = ["# Timing Graph Lab — results summary", ""]
    md += [f"- rows: {len(rows)}; commits: {', '.join(sorted({r['git_commit'] for r in rows}))}",
           f"- CPU: {', '.join(sorted({r['cpu_model'] for r in rows}))}; build: {', '.join(sorted({r['build_type'] for r in rows}))}",
           f"- seeds: {', '.join(sorted({r['seed'] for r in rows}))}; times are medians over repetitions (µs), "
           "then median over seeds; `spread` = (max-min)/median of per-seed medians", ""]
    bad = [r for r in rows if not r["pass"]]
    md += ["## Gate D — correctness", ""]
    if bad:
        md += [f"**{len(bad)} row(s) failed verification and are excluded from all tables:**", ""]
        md += [table(["run_id", "topology", "nodes", "repr", "phase", "algorithm", "threads", "max_abs_err"],
                     [[r["run_id"], r["topology"], r["node_count"], r["representation"], r["phase"], r["algorithm"],
                       r["thread_count"], r["max_abs_err"]] for r in bad[:30]]), ""]
    else:
        md += ["All rows passed verification against the reference engine.", ""]
    nonbit = sorted({(r["phase"], r["algorithm"]) for r in rows if r["pass"] and r["bitwise_equal"] == "false"
                     and r["phase"] in ("arrival", "required", "slack", "incremental", "full_recompute")})
    md += [f"Results not bit-identical to the reference (but within tolerance): {nonbit or 'none'}", ""]

    md += ["## Experiment A — Adjacency List vs CSR", ""]
    body = []
    for g in graphs(S):
        for phase, alg_l, alg_c in (("build", "push_back", "counting_sort"), ("topo_sort", "kahn", "kahn"),
                                    ("arrival", "pull", "pull"), ("arrival", "push", "push"),
                                    ("required", "pull", "pull"), ("edge_insert", "append", "rebuild")):
            a, c = get(S, *g, "list", phase, alg_l), get(S, *g, "csr", phase, alg_c)
            if a and c:
                body.append([*g[:2], int(a["edges"]), f"{phase}/{alg_c if phase != 'edge_insert' else 'append|rebuild'}",
                             us(a["median"]), us(c["median"]), f"{a['median'] / c['median']:.2f}",
                             f"{max(a['seed_spread'], c['seed_spread']) * 100:.0f}%"])
        a, c = get(S, *g, "list", "build", "push_back"), get(S, *g, "csr", "build", "counting_sort")
        if a and c:
            body.append([*g[:2], int(a["edges"]), "structure MiB", f"{a['mem'] / 2**20:.1f}", f"{c['mem'] / 2**20:.1f}",
                         f"{a['mem'] / c['mem']:.2f}", ""])
    md += [table(["topology", "nodes", "edges", "phase", "list µs", "csr µs", "list/csr", "seed spread"], body), ""]
    md += ["CSR break-even: number of full arrival+required passes after which CSR's (build) cost is repaid:", ""]
    body = []
    for g in graphs(S):
        b = [get(S, *g, r, "build", a) for r, a in (("list", "push_back"), ("csr", "counting_sort"))]
        p = [[get(S, *g, r, ph, "pull") for ph in ("arrival", "required")] for r in ("list", "csr")]
        if all(b) and all(all(x) for x in p):
            pl, pc = sum(x["median"] for x in p[0]), sum(x["median"] for x in p[1])
            extra = b[1]["median"] - b[0]["median"]
            gain = pl - pc
            be = "already cheaper to build" if extra <= 0 else (f"{extra / gain:.1f} passes" if gain > 0 else "never")
            body.append([*g[:2], us(b[0]["median"]), us(b[1]["median"]), us(pl), us(pc), be])
    md += [table(["topology", "nodes", "build list", "build csr", "AT+RT list", "AT+RT csr", "break-even"], body), ""]

    md += ["## Experiment B — topology (CSR, serial pull)", ""]
    body = []
    for g in graphs(S):
        at = get(S, *g, "csr", "arrival", "pull")
        if at:
            body.append([*g[:2], int(at["edges"]), at["sources"], at["sinks"], at["max_fanout"], at["max_fanin"],
                         at["depth"], at["lw_median"], us(at["median"]), f"{at['edges'] / at['median'] * 1e6:.3e}",
                         f"{at['cv'] * 100:.1f}%"])
    md += [table(["topology", "nodes", "edges", "sources", "sinks", "max fo", "max fi", "depth", "median level width",
                  "arrival µs", "edges/s", "CV"], body), ""]

    md += ["## Experiment C — incremental vs full recompute (CSR)", "",
           "S_inc = T_full / T_incremental (> 1: incremental faster). affected = nodes in the fanout cone (AT) "
           "∪ fanin cone (RT) of the edited edges; changed = nodes whose AT or RT changed.", ""]
    body = []
    keys = sorted({(k[0], k[1], k[2], k[7], k[8]) for k in S if k[4] == "incremental"})
    for (topo, n, f, ratio, dist) in keys:
        full = get(S, topo, n, f, "csr", "full_recompute", "pull", 1, ratio, dist)
        for alg in ("conservative", "early_stop"):
            inc = get(S, topo, n, f, "csr", "incremental", alg, 1, ratio, dist)
            if inc and full:
                body.append([topo, n, dist, f"{ratio * 100:g}%", inc["updated"], alg, int(inc["affected"]),
                             int(inc["work"]), int(inc["changed"]), us(inc["median"]), us(full["median"]),
                             f"{full['median'] / inc['median']:.2f}" if inc["median"] > 0 else "inf"])
    md += [table(["topology", "nodes", "dist", "ratio", "edges", "mode", "affected", "evaluated", "changed",
                  "inc µs", "full µs", "S_inc"], body), ""]

    md += ["## Experiment D — level-parallel propagation (CSR)", "",
           "Speedup(p) = T_1 / T_p with T_1 = serial pull AT+RT; efficiency = speedup / p. "
           "Algorithm-only = arrival+required; end-to-end adds levelize + pool_start.", ""]
    body = []
    algs = sorted({k[5] for k in S if k[5].startswith("level_parallel")})
    for g in graphs(S):
        ser = [get(S, *g, "csr", ph, "pull") for ph in ("arrival", "required")]
        lv = get(S, *g, "csr", "levelize", "longest_hop")
        if not all(ser):
            continue
        t1 = sum(x["median"] for x in ser)
        for alg in algs:
            for t in sorted({k[6] for k in S if k[5] == alg}):
                par = [get(S, *g, "csr", ph, alg, t) for ph in ("arrival", "required")]
                ps = get(S, *g, "csr", "pool_start", alg, t)
                if all(par):
                    tp = sum(x["median"] for x in par)
                    e2e = tp + (lv["median"] if lv else 0) + (ps["median"] if ps else 0)
                    body.append([*g[:2], ser[0]["depth"], ser[0]["lw_median"], alg.replace("level_parallel_", ""), t,
                                 us(tp), f"{t1 / tp:.2f}", f"{t1 / tp / t * 100:.0f}%", us(e2e), f"{t1 / e2e:.2f}",
                                 f"{tp / max(ser[0]['depth'], 1):.2f}"])
    md += [table(["topology", "nodes", "depth", "median width", "barrier", "threads", "AT+RT µs", "speedup", "efficiency",
                  "end-to-end µs", "e2e speedup", "µs/level"], body), ""]

    md += ["## Experiment E — traversal (CSR)", "",
           "Traversal cost only; BFS does not produce a timing order in general (column 'BFS order valid'). "
           "The last two columns run the same pull propagation over Kahn vs DFS orders.", ""]
    body = []
    for g in graphs(S):
        k, d, b = (get(S, *g, "csr", ph, a) for ph, a in (("topo_sort", "kahn"), ("topo_sort", "dfs"), ("traversal", "bfs")))
        ki = get(S, *g, "csr", "topo_indegree", "kahn")
        ak, ad = get(S, *g, "csr", "arrival", "pull"), get(S, *g, "csr", "arrival", "pull_dfs_order")
        if k and d and b:
            valid = "yes" if "bfs_order_is_topological=1" in b["notes"] else "no"
            body.append([*g[:2], us(k["median"]), us(ki["median"]) if ki else "-", us(d["median"]), us(b["median"]),
                         f"{k['mem'] / 2**20:.1f}/{d['mem'] / 2**20:.1f}/{b['mem'] / 2**20:.1f}", valid,
                         us(ak["median"]) if ak else "-", us(ad["median"]) if ad else "-"])
    md += [table(["topology", "nodes", "kahn µs", "(indegree init)", "dfs µs", "bfs µs", "aux MiB k/d/b",
                  "BFS order valid", "AT kahn-order", "AT dfs-order"], body), ""]

    md += ["## Memory and noise", "",
           "peak_rss_bytes is process-wide (the whole run, incl. reference engine and both representations); "
           "structure memory is the computed size of the graph arrays.", ""]
    rss = defaultdict(int)
    for r in rows:
        key = (r["topology"], r["node_count"])
        rss[key] = max(rss[key], r["peak_rss_bytes"])
    md += [table(["topology", "nodes", "peak RSS MiB"], [[t, n, f"{v / 2**20:.1f}"] for (t, n), v in sorted(rss.items())]), ""]
    noisy = [k for k, v in S.items() if v["cv"] > 0.10 and v["median"] > 100]
    md += [f"{len(noisy)} configuration(s) with CV > 10% (median > 100 µs)" +
           (": " + "; ".join(f"{k[0]}/{k[1]}/{k[3]}/{k[4]}/{k[5]}/t{k[6]}" for k in noisy[:15]) if noisy else ""), ""]
    return "\n".join(md)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv", nargs="+")
    ap.add_argument("--out")
    ap.add_argument("--summary", help="write per-configuration statistics as CSV")
    args = ap.parse_args()
    rows = load(args.csv)
    if not rows:
        sys.exit("no rows")
    S = summarize(rows)
    md = report(rows, S)
    if args.out:
        open(args.out, "w").write(md + "\n")
    else:
        print(md)
    if args.summary:
        cols = ("seeds", "n", "median", "p90", "min", "max", "cv", "seed_spread", "edges", "work", "mem", "affected", "changed")
        with open(args.summary, "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(list(CFG) + [c if c not in ("median", "p90", "min", "max") else c + "_us" for c in cols])
            for k, v in sorted(S.items(), key=lambda kv: tuple(map(str, kv[0]))):
                w.writerow(list(k) + [v[c] for c in cols])
    return 1 if any(not r["pass"] for r in rows) else 0


if __name__ == "__main__":
    sys.exit(main())
