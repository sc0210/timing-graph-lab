# Timing Graph Lab environment. Source it (do not execute it):
#
#   source env.sh                          # uses ./build (Release)
#   TGL_BUILD_DIR=build-tsan source env.sh  # point at another build directory
#
# Afterwards, from any directory:
#   timing_graph_bench --help              # the benchmark binary
#   run_suite.py configs/smoke.json        # the scripts in scripts/
#   tgl_build / tgl_test / tgl_suite NAME / tgl_info / tgl_unload
#
# Works in bash and zsh. Sourcing twice is safe; tgl_unload restores the previous PATH.

if [ -n "${BASH_SOURCE:-}" ]; then
  _tgl_self="${BASH_SOURCE[0]}"
elif [ -n "${ZSH_VERSION:-}" ]; then
  _tgl_self="${(%):-%x}"
else
  _tgl_self="$0"
fi
case "$_tgl_self" in
  env.sh | */env.sh) ;;
  *) echo "env.sh: source this file from bash or zsh" >&2; unset _tgl_self; return 1 2>/dev/null || exit 1 ;;
esac
if [ -n "${BASH_SOURCE:-}" ] && [ "${BASH_SOURCE[0]}" = "$0" ]; then
  echo "env.sh: this file must be sourced: source env.sh" >&2
  exit 1
fi

# Re-sourcing: undo the previous setup first so PATH entries never accumulate
# (keeping a TGL_BUILD_DIR the caller just asked for).
_tgl_want_build="${TGL_BUILD_DIR:-build}"
if typeset -f tgl_unload >/dev/null 2>&1; then tgl_unload --quiet; fi

export TGL_ROOT="$(cd "$(dirname "$_tgl_self")" && pwd)"
unset _tgl_self
case "$_tgl_want_build" in
  /*) export TGL_BUILD_DIR="$_tgl_want_build" ;;
  *) export TGL_BUILD_DIR="$TGL_ROOT/$_tgl_want_build" ;;
esac
unset _tgl_want_build
export TGL_RESULTS_DIR="$TGL_ROOT/results"
export TGL_BENCH="$TGL_BUILD_DIR/timing_graph_bench"

_TGL_OLD_PATH="$PATH"
export PATH="$TGL_BUILD_DIR:$TGL_ROOT/scripts:$PATH"

# Sanitizer builds: stop on the first report. Values already set by the user are kept
# (and are not touched by tgl_unload); only the defaults set here are removed again.
_TGL_SET_VARS=""
for _tgl_v in ASAN_OPTIONS UBSAN_OPTIONS TSAN_OPTIONS PYTHONDONTWRITEBYTECODE; do
  eval "[ -z \"\${$_tgl_v+x}\" ]" && _TGL_SET_VARS="$_TGL_SET_VARS $_tgl_v"
done
unset _tgl_v
export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=1:detect_leaks=1}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=1:print_stacktrace=1}"
export TSAN_OPTIONS="${TSAN_OPTIONS:-halt_on_error=1:second_deadlock_stack=1}"
# Keep the scripts from writing __pycache__ into the source tree.
export PYTHONDONTWRITEBYTECODE=1

# Configure and build. Usage: tgl_build [Release|Debug|RelWithDebInfo] [extra cmake args...]
# The build type defaults to Release; for build-asan / build-tsan the sanitizer is set automatically.
tgl_build() {
  _tgl_type="${1:-Release}"
  [ $# -gt 0 ] && shift
  _tgl_san=""
  case "$(basename "$TGL_BUILD_DIR")" in
    build-asan) _tgl_san="-DTGL_SANITIZER=address" ;;
    build-tsan) _tgl_san="-DTGL_SANITIZER=thread" ;;
  esac
  cmake -S "$TGL_ROOT" -B "$TGL_BUILD_DIR" -DCMAKE_BUILD_TYPE="$_tgl_type" $_tgl_san "$@" &&
    cmake --build "$TGL_BUILD_DIR" -j
  _tgl_rc=$?
  unset _tgl_type _tgl_san
  return $_tgl_rc
}

# Run the test suites of the current build directory. Extra args go to ctest (e.g. -L unit).
tgl_test() {
  ( cd "$TGL_BUILD_DIR" && ctest --output-on-failure "$@" )
}

# Run a suite by config name or path: tgl_suite smoke  ==  run_suite.py configs/smoke.json
tgl_suite() {
  if [ $# -lt 1 ]; then echo "usage: tgl_suite NAME|CONFIG.json [run_suite.py options]" >&2; return 2; fi
  _tgl_cfg="$1"
  shift
  [ -f "$_tgl_cfg" ] || _tgl_cfg="$TGL_ROOT/configs/${_tgl_cfg%.json}.json"
  python3 "$TGL_ROOT/scripts/run_suite.py" "$_tgl_cfg" --build-dir "$TGL_BUILD_DIR" "$@"
  _tgl_rc=$?
  unset _tgl_cfg
  return $_tgl_rc
}

tgl_info() {
  echo "TGL_ROOT        = $TGL_ROOT"
  echo "TGL_BUILD_DIR   = $TGL_BUILD_DIR"
  echo "TGL_RESULTS_DIR = $TGL_RESULTS_DIR"
  if [ -x "$TGL_BENCH" ]; then
    echo "TGL_BENCH       = $TGL_BENCH"
    _tgl_bt="$(sed -n 's/^CMAKE_BUILD_TYPE:STRING=//p' "$TGL_BUILD_DIR/CMakeCache.txt" 2>/dev/null)"
    echo "build type      = ${_tgl_bt:-unknown}"
    unset _tgl_bt
  else
    echo "TGL_BENCH       = (not built yet: run tgl_build)"
  fi
}

# Undo everything env.sh set.
tgl_unload() {
  [ -n "${_TGL_OLD_PATH:-}" ] && export PATH="$_TGL_OLD_PATH"
  case "${_TGL_SET_VARS:-}" in *" ASAN_OPTIONS"*) unset ASAN_OPTIONS ;; esac
  case "${_TGL_SET_VARS:-}" in *" UBSAN_OPTIONS"*) unset UBSAN_OPTIONS ;; esac
  case "${_TGL_SET_VARS:-}" in *" TSAN_OPTIONS"*) unset TSAN_OPTIONS ;; esac
  case "${_TGL_SET_VARS:-}" in *" PYTHONDONTWRITEBYTECODE"*) unset PYTHONDONTWRITEBYTECODE ;; esac
  unset _tgl_v TGL_ROOT TGL_BUILD_DIR TGL_RESULTS_DIR TGL_BENCH _TGL_OLD_PATH _TGL_SET_VARS
  unset -f tgl_build tgl_test tgl_suite tgl_info tgl_unload
  [ "${1:-}" = "--quiet" ] || echo "timing-graph-lab environment unloaded"
}

if [ -x "$TGL_BENCH" ]; then
  echo "timing-graph-lab: $TGL_BENCH ready (tgl_info for details)"
else
  echo "timing-graph-lab: environment set; $TGL_BENCH not built yet, run: tgl_build"
fi
