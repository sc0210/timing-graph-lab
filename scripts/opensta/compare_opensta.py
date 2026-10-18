#!/usr/bin/env python3
"""Cross-check timing_graph_sta against OpenSTA on a reduced-semantics netlist.

OpenSTA (https://github.com/The-OpenROAD-Project/OpenSTA, GPL-3.0) is run as an external
program and is never linked into this repository. This script writes collateral in which
OpenSTA's full STA semantics reduce exactly to the lab's model (one delay per edge,
max-plus arrival, min-plus required):

  * timing node v with fanin   -> instance u<v> of cell TGL_K<k> (k inputs, output Z),
                                  positive unate, zero cell delay and zero transition, so rise
                                  and fall behave identically and slew has no effect;
  * timing node v without fanin -> input port n<v>; a sink with fanin drives output port n<v>;
  * edge u -> v (delay d)       -> net from u's driver to an input pin of u<v>, with the delay
                                  annotated as an SDF INTERCONNECT;
  * constraints                 -> virtual clock with period P = required time of the lab run,
                                  input delay 0 on every input, output delay 0 on every output.

It then reads every node's max arrival, max required time and max slack back from OpenSTA
and compares them with timing_graph_sta's per-node CSV. Isolated nodes (no fanin and no
fanout) have no OpenSTA counterpart and are skipped.

Precision. OpenSTA computes in single precision, with times in seconds: an SDF value d
becomes float32(float32(d) * float32(1e-9)), and reports convert back by float division.
Comparing its float results with the lab's double results directly would need a tolerance
that grows with path length (≈ 0.2 ns on a 2,000-deep chain), loose enough to hide real
errors. The check is therefore split in two, around an independent Python implementation of
the lab's timing model (max-plus arrival, min-plus required, both in DP order):

  A. model in double  vs timing_graph_sta    tolerance abs 1e-9 + rel 1e-12 (expected bit-exact)
  B. model in float32 vs OpenSTA             tolerance 8 float32 ulps of the graph's time scale
                                             T = max |AT|, |RT| over all nodes, plus 1e-6 for the
                                             printed rounding of slack
     (the float32 model uses OpenSTA's own conversions, documented in sdf/SdfReader.cc)

Measured on chain / layered / reconvergent / random graphs up to 20K nodes and ISCAS c17:
check A is exact (difference 0); in check B arrival times agree to the printed digits and
required times within 2 ulps of T. That residual comes from an operation order inside
OpenSTA's required-time computation that the model does not reproduce. 8 ulps of T is
≈ 0.004 ns at T = 4,445 ns and ≈ 0.1 ns at T = 99,000 ns, so a real 1 ns error still fails
(check it with --perturb-edge).

The raw OpenSTA-vs-timing_graph_sta difference is printed for information.

--perturb-edge K adds 1 to edge K's delay in the OpenSTA collateral only. The run must then
report mismatches; it proves the comparison can fail (use it when changing this script).

usage:
  scripts/opensta/compare_opensta.py INPUT --sta PATH/TO/sta [--tgl build/timing_graph_sta]
         [--work DIR] [--keep] [--perturb-edge K]
  INPUT: an ISCAS .bench netlist or a tgl-edgelist file
exit status: 0 = every node matches, 1 = mismatches, 2 = setup or tool error
"""
import argparse
import csv
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read_edge_list(path):
    tok = Path(path).read_text().split()
    if tok[:2] != ["tgl-edgelist", "1"]:
        sys.exit(f"{path}: not a tgl-edgelist v1 file")
    n, m = int(tok[2]), int(tok[3])
    edges = [(int(tok[4 + 3 * i]), int(tok[5 + 3 * i]), float(tok[6 + 3 * i])) for i in range(m)]
    return n, edges


def write_collateral(work, n, edges, period):
    fanin = [[] for _ in range(n)]  # per node: list of (edge index, source)
    fanout = [0] * n
    for i, (u, v, _) in enumerate(edges):
        fanin[v].append((i, u))
        fanout[u] += 1
    nodes = [v for v in range(n) if fanin[v] or fanout[v]]
    inputs = [v for v in nodes if not fanin[v]]
    outputs = [v for v in nodes if fanin[v] and not fanout[v]]
    kmax = max((len(f) for f in fanin), default=0)

    def driver(u):
        return f"n{u}" if not fanin[u] else f"u{u}/Z"

    # Liberty: TGL_K1 .. TGL_K<kmax>, positive unate OR cells with zero delay and transition.
    zero = 'values ("0");'
    lib = ['library (tgl) {', '  delay_model : table_lookup;', '  time_unit : "1ns";', '  voltage_unit : "1V";',
           '  current_unit : "1mA";', '  capacitive_load_unit (1, pf);', '  pulling_resistance_unit : "1kohm";']
    for pct in ("input_threshold_pct", "output_threshold_pct"):
        lib += [f"  {pct}_rise : 50;", f"  {pct}_fall : 50;"]
    lib += ["  slew_lower_threshold_pct_rise : 20;", "  slew_upper_threshold_pct_rise : 80;",
            "  slew_lower_threshold_pct_fall : 20;", "  slew_upper_threshold_pct_fall : 80;"]
    for k in range(1, kmax + 1):
        lib.append(f"  cell (TGL_K{k}) {{")
        for i in range(k):
            lib.append(f"    pin (A{i}) {{ direction : input; capacitance : 0; }}")
        func = " | ".join(f"A{i}" for i in range(k))
        lib.append(f'    pin (Z) {{ direction : output; function : "{func}";')
        for i in range(k):
            lib.append(f'      timing () {{ related_pin : "A{i}"; timing_sense : positive_unate; '
                       f"cell_rise (scalar) {{ {zero} }} cell_fall (scalar) {{ {zero} }} "
                       f"rise_transition (scalar) {{ {zero} }} fall_transition (scalar) {{ {zero} }} }}")
        lib += ["    }", "  }"]
    lib.append("}")
    (work / "tgl.lib").write_text("\n".join(lib) + "\n")

    # Verilog: net n<v> is node v's driver net (an input port, or u<v>/Z; also the output port for sinks).
    ports = [f"n{v}" for v in inputs + outputs]
    ver = [f"module top ({', '.join(ports)});"]
    if inputs:
        ver.append(f"  input {', '.join(f'n{v}' for v in inputs)};")
    if outputs:
        ver.append(f"  output {', '.join(f'n{v}' for v in outputs)};")
    internal = [v for v in nodes if fanin[v] and fanout[v]]
    if internal:
        ver.append(f"  wire {', '.join(f'n{v}' for v in internal)};")
    sdf_lines = []
    for v in nodes:
        if not fanin[v]:
            continue
        conns = []
        for i, (e, u) in enumerate(fanin[v]):
            conns.append(f".A{i}(n{u})")
            d = repr(edges[e][2])
            sdf_lines.append(f"      (INTERCONNECT {driver(u)} u{v}/A{i} ({d}:{d}:{d}))")
        conns.append(f".Z(n{v})")
        ver.append(f"  TGL_K{len(fanin[v])} u{v} ({', '.join(conns)});")
    ver.append("endmodule")
    (work / "top.v").write_text("\n".join(ver) + "\n")

    sdf = ['(DELAYFILE', '  (SDFVERSION "3.0")', '  (DESIGN "top")', "  (TIMESCALE 1ns)",
           '  (CELL (CELLTYPE "top") (INSTANCE)', "    (DELAY (ABSOLUTE"] + sdf_lines + ["    ))", "  )", ")"]
    (work / "top.sdf").write_text("\n".join(sdf) + "\n")

    # Query script. report_arrival / report_required print to stdout themselves, so each node
    # emits four lines: "TGL <node>", the arrival report, the required report, "SLACK <value>".
    tcl = ["read_liberty tgl.lib", "read_verilog top.v", "link_design top", "read_sdf top.sdf",
           f"create_clock -name vclk -period {period!r}",
           "set_input_delay 0 -clock vclk [all_inputs]", "set_output_delay 0 -clock vclk [all_outputs]"]
    for v in nodes:
        obj = f"[get_ports n{v}]" if not fanin[v] else f"[get_pins u{v}/Z]"
        tcl += [f'puts "TGL {v}"', f"report_arrival -digits 9 {obj}", f"report_required -digits 9 {obj}",
                f'puts "SLACK [get_property {obj} slack_max]"']
    (work / "run.tcl").write_text("\n".join(tcl) + "\n")
    return nodes


MINMAX = re.compile(r"r\s+(\S+):(\S+)\s+f\s+(\S+):(\S+)")


def parse_report(text):
    """'(vclk ^) r min:max f min:max' -> (max rise, max fall)."""
    m = MINMAX.search(text)
    if not m:
        raise ValueError(f"cannot parse OpenSTA report '{text.strip()}'")
    return float(m.group(2)), float(m.group(4))


def f32(x):
    """Round a Python float to IEEE single precision."""
    return struct.unpack("f", struct.pack("f", x))[0]


NS32 = f32(1e-9)


def model(n, edges, period, single):
    """The lab's timing model, independent of the C++ engine. single=False: double precision,
    times in ns. single=True: OpenSTA's float32 arithmetic, times in seconds."""
    fanin = [[] for _ in range(n)]
    fanout = [[] for _ in range(n)]
    for u, v, d in edges:
        d = f32(f32(d) * NS32) if single else d
        fanin[v].append((u, d))
        fanout[u].append((v, d))
    rnd = f32 if single else (lambda x: x)
    indeg = [len(f) for f in fanin]
    order = [v for v in range(n) if indeg[v] == 0]
    for v in order:  # Kahn; `order` grows while iterating
        for w, _ in fanout[v]:
            indeg[w] -= 1
            if indeg[w] == 0:
                order.append(w)
    if len(order) != n:
        raise ValueError("graph has a cycle")
    at, rt = [0.0] * n, [0.0] * n
    for v in order:
        if fanin[v]:
            at[v] = max(rnd(at[u] + d) for u, d in fanin[v])
    p = f32(period * 1e-9) if single else period
    for v in reversed(order):
        rt[v] = min(rnd(rt[w] - d) for w, d in fanout[v]) if fanout[v] else p
    slack = [rnd(r - a) for a, r in zip(at, rt)]
    if single:  # back to ns the way OpenSTA reports
        at, rt, slack = ([f32(x / NS32) for x in vec] for vec in (at, rt, slack))
    return at, rt, slack


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("input")
    ap.add_argument("--sta", required=True, help="OpenSTA binary")
    ap.add_argument("--tgl", default=str(ROOT / "build" / "timing_graph_sta"))
    ap.add_argument("--work", help="directory for the generated collateral (default: temporary)")
    ap.add_argument("--keep", action="store_true", help="keep the temporary directory")
    ap.add_argument("--perturb-edge", help="self-check: add 1 to this edge's delay for OpenSTA only; "
                    "an edge id, or 'critical' for an edge on a zero-slack path (guaranteed to change timing)")
    args = ap.parse_args()

    work = Path(args.work) if args.work else Path(tempfile.mkdtemp(prefix="tgl-opensta-"))
    work.mkdir(parents=True, exist_ok=True)
    try:
        # 1. The lab's result and the exact graph it timed.
        cmd = [args.tgl, args.input, "--out", str(work / "tgl.csv"), "--write-edgelist", str(work / "graph.el"),
               "--write-names", str(work / "names.txt")]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            print(r.stdout + r.stderr, file=sys.stderr)
            return 2
        ours = {int(row["node"]): row for row in csv.DictReader(open(work / "tgl.csv"))}
        names = (work / "names.txt").read_text().split("\n")
        n, edges = read_edge_list(work / "graph.el")
        has_in, has_out = [False] * n, [False] * n
        for u, v, _ in edges:
            has_out[u], has_in[v] = True, True
        # Every sink carries the same required time (timing_graph_sta has no per-sink r0); it
        # becomes the OpenSTA clock period.
        rt = {float(ours[v]["rt"]) for v in range(n) if has_in[v] and not has_out[v]}
        if len(rt) != 1:
            print(f"expected one common sink required time, found {len(rt)}", file=sys.stderr)
            return 2
        period = rt.pop()
        if period <= 0:
            print("required time must be positive for an OpenSTA clock period", file=sys.stderr)
            return 2

        # 2. OpenSTA on the equivalent netlist.
        edges_lab = list(edges)
        if args.perturb_edge is not None:
            if args.perturb_edge == "critical":
                at = {v: float(row["at"]) for v, row in ours.items()}
                slack = {v: float(row["slack"]) for v, row in ours.items()}
                wns = min(slack.values())
                k = next(i for i, (u, v, d) in enumerate(edges) if slack[v] == wns and at[u] + d == at[v])
            else:
                k = int(args.perturb_edge)
            u, v, d = edges[k]
            edges[k] = (u, v, d + 1)
            print(f"self-check: edge {k} ({u} -> {v}) delay {d!r} -> {d + 1!r} for OpenSTA only; expect MISMATCHES")
        nodes = write_collateral(work, n, edges, period)
        r = subprocess.run([args.sta, "-no_splash", "-exit", "run.tcl"], cwd=work, capture_output=True, text=True)
        (work / "sta.log").write_text(r.stdout + r.stderr)
        if r.returncode != 0 or "Error" in r.stdout + r.stderr:
            print(f"OpenSTA failed (see {work / 'sta.log'}):\n" + (r.stdout + r.stderr)[-2000:], file=sys.stderr)
            return 2
        theirs = {}
        lines = r.stdout.splitlines()
        for i, line in enumerate(lines):
            if line.startswith("TGL "):
                arr, req, slack = lines[i + 1], lines[i + 2], lines[i + 3]
                if not slack.startswith("SLACK "):
                    print(f"unexpected OpenSTA output near line {i + 1} (see {work / 'sta.log'})", file=sys.stderr)
                    return 2
                # Max arrival over rise/fall; max (setup) required is the min over rise/fall.
                theirs[int(line[4:])] = (max(parse_report(arr)), min(parse_report(req)), float(slack[6:]))
        if set(theirs) != set(nodes):
            print(f"OpenSTA reported {len(theirs)} of {len(nodes)} nodes", file=sys.stderr)
            return 2

        # 3. Compare every node: A = lab engine vs double model, B = OpenSTA vs float32 model.
        d64 = model(n, edges_lab, period, single=False)
        d32 = model(n, edges_lab, period, single=True)  # the lab's graph, never the perturbed one
        # Intermediate values reach the graph's largest time, so rounding is bounded by its ulp.
        scale = max(max(abs(x) for x in d32[0]), max(abs(x) for x in d32[1]))
        tol_b = 8 * 2.0**-23 * scale + 1e-6
        bad, worst = [], {"A": 0.0, "B": 0.0, "raw": 0.0}
        for v in nodes:
            o = ours[v]
            mine = (float(o["at"]), float(o["rt"]), float(o["slack"]))
            for k, what in enumerate(("at", "rt", "slack")):
                a, m64, m32, b = mine[k], d64[k][v], d32[k][v], theirs[v][k]
                err_a, err_b = abs(a - m64), abs(b - m32)
                worst["A"], worst["B"] = max(worst["A"], err_a), max(worst["B"], err_b)
                worst["raw"] = max(worst["raw"], abs(a - b))
                label = names[v] if v < len(names) else str(v)
                if not err_a <= 1e-9 + 1e-12 * max(abs(a), abs(m64)):
                    bad.append(f"  A {label}: {what} timing_graph_sta={a!r} model={m64!r}")
                if not err_b <= tol_b:
                    bad.append(f"  B {label}: {what} opensta={b!r} float32 model={m32!r}")
        skipped = n - len(nodes)
        print(f"{args.input}: {len(nodes)} nodes ({skipped} isolated skipped), {len(edges)} edges, required time "
              f"{period:g}: max |engine - model| {worst['A']:.3g}, max |OpenSTA - float32 model| {worst['B']:.3g} "
              f"(tolerance {tol_b:.3g}) "
              f"(raw |engine - OpenSTA| {worst['raw']:.3g}); {'MATCH' if not bad else f'{len(bad)} MISMATCHES'}")
        for b in bad[:20]:
            print(b)
        return 1 if bad else 0
    finally:
        if not args.work and not args.keep:
            shutil.rmtree(work, ignore_errors=True)
        elif args.keep:
            print(f"collateral kept in {work}")


if __name__ == "__main__":
    sys.exit(main())
