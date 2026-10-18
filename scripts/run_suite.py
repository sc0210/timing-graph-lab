#!/usr/bin/env python3
"""Run an experiment suite described by a JSON config (plan §10 SOP).

  1. Quality gates (plan §7.5) before any measurement:
       Gate A/B  ctest in the Release build (unit, property, differential, regression)
       Gate C    ctest in build-asan / build-tsan when those builds exist (--require-sanitizers
                 makes them mandatory)
     A failed gate aborts the suite; --skip-gates exists only for debugging and tags results.
  2. Each list-valued key of "matrix" is expanded as a cartesian product ("matrices": [...]
     concatenates several such products, e.g. to cap one topology's size); every point runs
     timing_graph_bench in a fresh process (so peak RSS is per configuration) and appends
     rows to one CSV.  Gate D is enforced per run by the benchmark (correctness_pass column,
     exit code 2) and again by analyze_results.py.
  3. A sidecar .meta.json records commit, dirty state, CPU, cores, RAM, OS, compiler,
     build type and flags, CPU governor, load average, thread placement, the config and
     its SHA-256, and the gate results.

usage: scripts/run_suite.py configs/baseline.json [--build-dir build] [--perf] [--dry-run]
"""
import argparse
import datetime as dt
import hashlib
import itertools
import json
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# matrix key -> timing_graph_bench flag
FLAGS = {
    "topology": "--topology", "nodes": "--nodes", "layers": "--layers", "span": "--span",
    "fanout": "--fanout", "seed": "--seed", "delay_dist": "--delay-dist",
    "representation": "--representation", "algorithm": "--algorithm", "threads": "--threads",
    "barrier": "--barrier", "update_ratio": "--update-ratio", "update_dist": "--update-dist",
    "inc_mode": "--inc-mode", "insert_ratio": "--insert-ratio",
    "source_arrival_spread": "--source-arrival-spread", "period": "--period",
    "oracle_max_edges": "--oracle-max-edges",
}


def sh(cmd, **kw):
    try:
        return subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, check=True, **kw).stdout.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return "unknown"


def read(path, default="unknown"):
    try:
        return Path(path).read_text().strip()
    except OSError:
        return default


def cpu_model():
    for line in read("/proc/cpuinfo", "").splitlines():
        if line.startswith("model name"):
            return line.split(":", 1)[1].strip()
    if sys.platform == "darwin":
        return sh(["sysctl", "-n", "machdep.cpu.brand_string"]) or platform.processor() or "unknown"
    return platform.processor() or "unknown"


def mem_total_bytes():
    for line in read("/proc/meminfo", "").splitlines():
        if line.startswith("MemTotal:"):
            return int(line.split()[1]) * 1024
    if sys.platform == "darwin":
        out = sh(["sysctl", "-n", "hw.memsize"])
        return int(out) if out and out.isdigit() else None
    return None


def loadavg():
    try:
        return " ".join(f"{x:.2f}" for x in os.getloadavg())
    except OSError:
        return "unknown"


def cmake_cache(build_dir, key):
    for line in read(build_dir / "CMakeCache.txt", "").splitlines():
        if line.startswith(key + ":"):
            return line.split("=", 1)[1].strip()
    return "unknown"


def expand(matrix):
    keys = list(matrix)
    values = [v if isinstance(v, list) else [v] for v in matrix.values()]
    for combo in itertools.product(*values):
        yield dict(zip(keys, combo))


def run_gate(name, build_dir, env):
    if not (build_dir / "CTestTestfile.cmake").exists():
        return None
    # Rebuild first so the tested binaries match the current source tree.
    if subprocess.run(["cmake", "--build", str(build_dir), "-j"], capture_output=True).returncode != 0:
        print(f"   build failed in {build_dir}")
        return False
    print(f"[gate] {name}: ctest in {build_dir.relative_to(ROOT)}", flush=True)
    r = subprocess.run(["ctest", "--output-on-failure"], cwd=build_dir, env=env, capture_output=True, text=True)
    tail = "\n".join(r.stdout.strip().splitlines()[-3:])
    print("   " + tail.replace("\n", "\n   "))
    return r.returncode == 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("config")
    ap.add_argument("--build-dir", default="build")
    ap.add_argument("--perf", action="store_true", help="wrap each run with `perf stat` (if installed)")
    ap.add_argument("--require-sanitizers", action="store_true", help="fail if build-asan/build-tsan are missing")
    ap.add_argument("--skip-gates", action="store_true", help="debug only: results are tagged gates_skipped")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    cfg_bytes = Path(args.config).read_bytes()
    cfg = json.loads(cfg_bytes)
    build_dir = (ROOT / args.build_dir).resolve()
    binary = build_dir / "timing_graph_bench"
    if not binary.exists():
        sys.exit(f"{binary} not found; build first: cmake -S . -B {args.build_dir} -DCMAKE_BUILD_TYPE=Release "
                 f"&& cmake --build {args.build_dir} -j")

    commit = sh(["git", "rev-parse", "--short=12", "HEAD"])
    dirty = sh(["git", "status", "--porcelain", "--untracked-files=no"]) not in ("", "unknown")
    build_type = cmake_cache(build_dir, "CMAKE_BUILD_TYPE")
    if build_type != "Release":
        print(f"warning: build type is {build_type}; timings are not representative", file=sys.stderr)
    if dirty:
        print("warning: uncommitted changes; results are tagged -dirty", file=sys.stderr)

    env = dict(os.environ)
    env.update({k: str(v) for k, v in cfg.get("env", {}).items()})

    gates = {}
    if not args.skip_gates and not args.dry_run:
        gates["A_B_release_tests"] = run_gate("A+B", build_dir, env)
        for name, d in (("C_asan_ubsan", "build-asan"), ("C_tsan", "build-tsan")):
            gates[name] = run_gate(name, ROOT / d, env)
            if gates[name] is None and args.require_sanitizers:
                sys.exit(f"gate {name}: {d} not built (see README: sanitizer builds)")
        failed = [k for k, v in gates.items() if v is False]
        if gates["A_B_release_tests"] is None:
            failed.append("A_B_release_tests (no tests in build dir)")
        if failed:
            sys.exit(f"quality gate(s) failed: {', '.join(failed)}; benchmark not run (plan §7.5)")

    stamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    out_dir = ROOT / "results"
    out_dir.mkdir(exist_ok=True)
    base = out_dir / f"{cfg['name']}_{stamp}"
    csv_path, stats_path, perf_dir = base.with_suffix(".csv"), Path(f"{base}.graphs.jsonl"), Path(f"{base}_perf")
    use_perf = args.perf or cfg.get("perf", False)
    if use_perf and not shutil.which("perf"):
        print("warning: perf not installed; hardware counters skipped", file=sys.stderr)
        use_perf = False

    meta = {
        "config": cfg, "config_path": str(args.config), "config_sha256": hashlib.sha256(cfg_bytes).hexdigest(),
        "started": stamp, "commit": commit, "dirty": dirty, "build_type": build_type,
        "cxx_compiler": cmake_cache(build_dir, "CMAKE_CXX_COMPILER"),
        "compiler_version": (sh([cmake_cache(build_dir, "CMAKE_CXX_COMPILER"), "--version"]) or "unknown").splitlines()[0],
        "cxx_flags": cmake_cache(build_dir, "CMAKE_CXX_FLAGS") + " " + cmake_cache(build_dir, f"CMAKE_CXX_FLAGS_{build_type.upper()}"),
        "cpu": cpu_model(), "logical_cpus": os.cpu_count(), "mem_total_bytes": mem_total_bytes(),
        "os": platform.platform(), "kernel": platform.release(),
        "cpu_governor": read("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"),
        "loadavg_at_start": loadavg(), "env": cfg.get("env", {}),
        "thread_strategy": "fixed LevelPool (std::thread) or TbbPool (oneTBB task_arena) per thread count, created once per measurement and timed as pool_start",
        "tbb_engines": cmake_cache(build_dir, "TGL_HAVE_TBB"),
        "perf": use_perf, "gates": gates if not args.skip_gates else "skipped",
    }

    matrices = cfg["matrices"] if "matrices" in cfg else [cfg["matrix"]]
    points = [p for m in matrices for p in expand(m)]
    failures = []
    for i, point in enumerate(points, 1):
        run_id = f"{cfg['name']}-{stamp}-{i:03d}"
        cmd = [str(binary), "--repetitions", str(cfg.get("repetitions", 10)), "--warmup", str(cfg.get("warmup", 2)),
               "--commit", commit + ("-dirty" if dirty else ""), "--output", str(csv_path),
               "--graph-stats", str(stats_path), "--run-id", run_id]
        if args.skip_gates:
            cmd += ["--notes", "gates_skipped"]
        for k, v in point.items():
            if k not in FLAGS:
                sys.exit(f"unknown matrix key {k!r} (known: {', '.join(FLAGS)})")
            cmd += [FLAGS[k], ",".join(map(str, v)) if isinstance(v, (list, tuple)) else str(v)]
        cmd += cfg.get("extra_args", [])
        if use_perf:
            perf_dir.mkdir(exist_ok=True)
            cmd = ["perf", "stat", "-x", ",", "-o", str(perf_dir / f"{run_id}.txt"),
                   "-e", "cycles,instructions,cache-references,cache-misses,branch-misses"] + cmd
        desc = " ".join(f"{k}={v}" for k, v in point.items())
        print(f"[{i}/{len(points)}] {run_id}  {desc}", flush=True)
        if args.dry_run:
            print("   ", " ".join(cmd))
            continue
        r = subprocess.run(cmd, cwd=ROOT, env=env, capture_output=True, text=True)
        if r.returncode != 0:
            # Missing results are recorded with their reason, never silently dropped (plan Experiment A).
            reason = {2: "verification failure", 1: "error"}.get(r.returncode, f"exit {r.returncode} (killed? OOM?)")
            failures.append({"run_id": run_id, "point": point, "returncode": r.returncode, "reason": reason,
                             "stderr_tail": r.stderr.strip().splitlines()[-5:]})
            print(f"   FAILED: {reason}: {r.stderr.strip().splitlines()[-1:] }", file=sys.stderr)

    meta["finished"] = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    meta["loadavg_at_end"] = loadavg()
    meta["failed_points"] = failures
    if not args.dry_run:
        Path(f"{base}.meta.json").write_text(json.dumps(meta, indent=2))
        print(f"wrote {csv_path.relative_to(ROOT)} (+ .meta.json, .graphs.jsonl); {len(failures)} failed point(s)")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
