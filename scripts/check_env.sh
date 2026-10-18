#!/bin/sh
# Checks the tools and libraries this lab uses, prints what is installed and what is
# missing, and offers to install the missing ones with the platform's package manager.
#
#   scripts/check_env.sh            check, then ask before installing anything
#   scripts/check_env.sh --yes      check and install everything missing without asking
#   scripts/check_env.sh --check    check only, never install (CI, scripts)
#   scripts/check_env.sh --quiet    like --check, but print the report only if something required is missing
#
# Exit status: 0 if every required tool is present, 1 otherwise. Optional tools never fail the check.
# Respects $CXX (the compiler CMake will use) and $PYTHON (default python3).
set -u

MODE=ask
for arg in "$@"; do
  case "$arg" in
    -y | --yes) MODE=yes ;;
    -c | --check) MODE=check ;;
    -q | --quiet) MODE=quiet ;;
    -h | --help)
      sed -n '2,12p' "$0" | sed 's/^# \{0,1\}//'
      exit 0
      ;;
    *)
      echo "check_env.sh: unknown option $arg (--help for usage)" >&2
      exit 2
      ;;
  esac
done
# Never prompt without a terminal (CI, pipes).
if [ "$MODE" = ask ] && { [ ! -t 0 ] || [ -n "${CI:-}" ]; }; then MODE=check; fi

CXX_BIN="${CXX:-c++}"
CXX_EXE="${CXX_BIN%% *}"  # CXX may carry flags, as CMake allows (e.g. "clang++ -stdlib=libc++")
PY="${PYTHON:-python3}"
MIN_CMAKE=3.20
MIN_PY=3.8
TMP=$(mktemp -d 2>/dev/null || mktemp -d -t tglcheck)
trap 'rm -rf "$TMP"' EXIT INT TERM

if [ -t 1 ]; then
  OK=$(printf '\033[32m[ok]\033[0m') NO=$(printf '\033[31m[--]\033[0m') OPT=$(printf '\033[33m[--]\033[0m')
  B=$(printf '\033[1m') N=$(printf '\033[0m')
else
  OK='[ok]' NO='[--]' OPT='[--]' B='' N=''
fi

have() { command -v "$1" >/dev/null 2>&1; }

# ver_ge A B: true if version A >= version B (numeric, dot-separated).
ver_ge() {
  awk -v a="$1" -v b="$2" 'BEGIN {
    na = split(a, x, "."); nb = split(b, y, ".")
    for (i = 1; i <= (na > nb ? na : nb); i++) {
      if (x[i] + 0 > y[i] + 0) exit 0
      if (x[i] + 0 < y[i] + 0) exit 1
    }
    exit 0 }'
}

first_version() { sed -n 's/[^0-9]*\([0-9][0-9]*\.[0-9][0-9.]*\).*/\1/p' | head -n 1; }

# ---------------------------------------------------------------- platform
OS=$(uname -s)
PM=none
if [ "$OS" = Darwin ]; then
  PLATFORM="macOS $(sw_vers -productVersion 2>/dev/null)"
  have brew && PM=brew
else
  PLATFORM="$OS"
  [ -r /etc/os-release ] && PLATFORM=$(. /etc/os-release && echo "${PRETTY_NAME:-$OS}")
  if have apt-get; then
    PM=apt
  elif have dnf; then
    PM=dnf
  elif have pacman; then
    PM=pacman
  fi
fi
SUDO=""
[ "$PM" != brew ] && [ "$PM" != none ] && [ "$(id -u)" != 0 ] && have sudo && SUDO="sudo "

# pkg KEY: package name(s) for the current package manager ("" = not installable this way).
pkg() {
  case "$PM:$1" in
    brew:cmake) echo cmake ;;
    brew:compiler | brew:make) echo "@xcode" ;;
    brew:python) echo python ;;
    brew:git) echo git ;;
    brew:tbb) echo tbb ;;
    brew:clang-format) echo clang-format ;;
    brew:matplotlib) echo "@pip" ;;
    apt:cmake) echo cmake ;;
    apt:compiler | apt:make) echo build-essential ;;
    apt:python) echo python3 ;;
    apt:git) echo git ;;
    apt:tbb) echo libtbb-dev ;;
    apt:clang-format) echo clang-format ;;
    apt:matplotlib) echo python3-matplotlib ;;
    dnf:cmake) echo cmake ;;
    dnf:compiler) echo gcc-c++ ;;
    dnf:make) echo make ;;
    dnf:python) echo python3 ;;
    dnf:git) echo git ;;
    dnf:tbb) echo tbb-devel ;;
    dnf:clang-format) echo clang-tools-extra ;;
    dnf:matplotlib) echo python3-matplotlib ;;
    pacman:cmake) echo cmake ;;
    pacman:compiler | pacman:make) echo base-devel ;;
    pacman:python) echo python ;;
    pacman:git) echo git ;;
    pacman:tbb) echo onetbb ;;
    pacman:clang-format) echo clang ;;
    pacman:matplotlib) echo python-matplotlib ;;
    *) echo "" ;;
  esac
}

# ---------------------------------------------------------------- checks
# Each check sets STATUS (ok|missing) and DETAIL.
REPORT="$TMP/report"
MISSING_REQ=""
MISSING_OPT=""
: >"$REPORT"

record() { # record required|optional KEY LABEL PURPOSE
  if [ "$STATUS" = ok ]; then
    printf '    %s %-16s %s\n' "$OK" "$3" "$DETAIL" >>"$REPORT"
  elif [ "$1" = required ]; then
    printf '    %s %-16s %s\n' "$NO" "$3" "$DETAIL" >>"$REPORT"
    MISSING_REQ="$MISSING_REQ $2"
  else
    printf '    %s %-16s %s (%s)\n' "$OPT" "$3" "$DETAIL" "${4:-}" >>"$REPORT"
    MISSING_OPT="$MISSING_OPT $2"
  fi
}

check_cmake() {
  if have cmake; then
    v=$(cmake --version | first_version)
    if ver_ge "$v" "$MIN_CMAKE"; then
      STATUS=ok DETAIL="$v"
    else
      STATUS=missing DETAIL="$v found, need >= $MIN_CMAKE"
    fi
  else
    STATUS=missing DETAIL="not found, need >= $MIN_CMAKE"
  fi
}

check_make() {
  if have make; then
    STATUS=ok DETAIL="make $(make --version 2>/dev/null | first_version)"
  elif have ninja; then
    STATUS=ok DETAIL="ninja $(ninja --version) (configure with -G Ninja)"
  else
    STATUS=missing DETAIL="neither make nor ninja found"
  fi
}

check_compiler() {
  if ! have "$CXX_EXE"; then
    STATUS=missing DETAIL="$CXX_BIN not found"
    return
  fi
  # shellcheck disable=SC2086
  name=$($CXX_BIN --version 2>/dev/null | head -n 1)
  # The features the lab relies on: designated initializers, <barrier>, <span>, std::atomic wait-free ops.
  cat >"$TMP/cxx20.cpp" <<'EOF'
#include <atomic>
#include <barrier>
#include <span>
#include <thread>
#include <vector>
struct P { int a = 0; int b = 0; };
int main() {
  P p{.a = 1, .b = 2};
  std::vector<int> v{p.a, p.b};
  std::span<const int> s(v);
  std::barrier<> bar(2);
  std::atomic<int> n{0};
  std::thread t([&] { n += s[0]; bar.arrive_and_wait(); });
  n += s[1];
  bar.arrive_and_wait();
  t.join();
  return n == 3 ? 0 : 1;
}
EOF
  # shellcheck disable=SC2086
  if $CXX_BIN -std=c++20 -pthread "$TMP/cxx20.cpp" -o "$TMP/cxx20" >"$TMP/cxx20.log" 2>&1 && "$TMP/cxx20"; then
    STATUS=ok DETAIL="$name ($CXX_BIN)"
  else
    STATUS=missing DETAIL="$CXX_BIN cannot build C++20 (<barrier>, <span>): $(head -n 1 "$TMP/cxx20.log")"
    [ -n "${CXX:-}" ] && DETAIL="$DETAIL; CXX is set, unset it or point it at a newer compiler"
  fi
}

check_python() {
  if have "$PY"; then
    v=$("$PY" -c 'import sys; print("%d.%d.%d" % sys.version_info[:3])' 2>/dev/null)
    if [ -n "$v" ] && ver_ge "$v" "$MIN_PY"; then
      STATUS=ok DETAIL="$v"
    else
      STATUS=missing DETAIL="${v:-unknown} found, need >= $MIN_PY"
    fi
  else
    STATUS=missing DETAIL="$PY not found, need >= $MIN_PY"
  fi
}

check_git() {
  if have git; then
    STATUS=ok DETAIL="$(git --version | first_version)"
  else
    STATUS=missing DETAIL="not found"
  fi
}

check_tbb() {
  # Compile and link against TBB, the same way CMake's find_package(TBB) result is used.
  flags=""
  if [ "$PM" = brew ]; then
    prefix=$(brew --prefix tbb 2>/dev/null)
    [ -n "$prefix" ] && [ -d "$prefix" ] && flags="-I$prefix/include -L$prefix/lib -Wl,-rpath,$prefix/lib"
  fi
  printf '#include <tbb/version.h>\n#include <cstdio>\nint main() { std::printf("%%d.%%d\\n", TBB_VERSION_MAJOR, TBB_VERSION_MINOR); }\n' \
    >"$TMP/tbb.cpp"
  # shellcheck disable=SC2086
  if have "$CXX_EXE" && $CXX_BIN -std=c++20 $flags "$TMP/tbb.cpp" -o "$TMP/tbb" -ltbb >/dev/null 2>&1; then
    STATUS=ok DETAIL="oneTBB $("$TMP/tbb")"
  else
    STATUS=missing DETAIL="not found"
  fi
}

check_matplotlib() {
  v=$("$PY" -c 'import matplotlib; print(matplotlib.__version__)' 2>/dev/null)
  if [ -n "$v" ]; then
    STATUS=ok DETAIL="$v"
  else
    STATUS=missing DETAIL="not found"
  fi
}

check_clang_format() {
  for c in clang-format-18 clang-format; do
    if have "$c"; then
      STATUS=ok DETAIL="$($c --version | first_version) ($c)"
      return
    fi
  done
  STATUS=missing DETAIL="not found"
}

run_checks() {
  MISSING_REQ="" MISSING_OPT=""
  : >"$REPORT"
  echo "  ${B}required${N}" >>"$REPORT"
  check_cmake; record required cmake cmake
  check_make; record required make "build tool"
  check_compiler; record required compiler "C++20 compiler"
  check_python; record required python python3
  echo "  ${B}optional${N}" >>"$REPORT"
  check_tbb; record optional tbb oneTBB "TBB engines: --barrier tbb,tbb_dag"
  check_matplotlib; record optional matplotlib matplotlib "figures: plot_results.py"
  check_git; record optional git git "records the commit of each run"
  check_clang_format; record optional clang-format clang-format "formatting check for contributors"
}

# ---------------------------------------------------------------- install plan
# install_cmds KEYS...: prints one shell command per line that installs those keys.
install_cmds() {
  pkgs="" xcode="" pip=""
  for k in "$@"; do
    p=$(pkg "$k")
    case "$p" in
      "") ;;
      @xcode) xcode=1 ;;
      @pip) pip=1 ;;
      *) case " $pkgs " in *" $p "*) ;; *) pkgs="$pkgs $p" ;; esac ;;
    esac
  done
  [ -n "$xcode" ] && echo "xcode-select --install"
  if [ -n "$pkgs" ]; then
    case "$PM" in
      brew) echo "brew install$pkgs" ;;
      apt) echo "${SUDO}apt-get update && ${SUDO}apt-get install -y$pkgs" ;;
      dnf) echo "${SUDO}dnf install -y$pkgs" ;;
      pacman) echo "${SUDO}pacman -S --needed --noconfirm$pkgs" ;;
    esac
  fi
  [ -n "$pip" ] && echo "$PY -m pip install --user matplotlib"
  return 0
}

print_report() {
  echo
  echo "${B}timing-graph-lab toolchain check${N}  ($PLATFORM; package manager: $PM)"
  cat "$REPORT"
  echo
  if [ -z "$MISSING_REQ" ] && [ -z "$MISSING_OPT" ]; then
    echo "Everything is installed."
    return
  fi
  if [ -n "$MISSING_REQ" ]; then
    echo "${B}Missing required:${N}$MISSING_REQ  -> the lab cannot be built until these are installed."
  else
    echo "All required tools are installed; the lab builds and runs."
  fi
  [ -n "$MISSING_OPT" ] && echo "Missing optional:$MISSING_OPT  -> only the features in parentheses are unavailable."
  if [ "$PM" = none ]; then
    echo
    echo "No supported package manager found (Homebrew, apt, dnf, pacman)."
    [ "$OS" = Darwin ] && echo "Install Homebrew from https://brew.sh, then run this script again."
    return
  fi
  echo
  echo "To install them:"
  # shellcheck disable=SC2086
  install_cmds $MISSING_REQ $MISSING_OPT | sed 's/^/    /'
}

# ---------------------------------------------------------------- main
run_checks
if [ "$MODE" = quiet ]; then
  [ -n "$MISSING_REQ" ] && print_report
  [ -z "$MISSING_REQ" ]
  exit $?
fi
print_report

if [ "$MODE" = check ] || [ "$PM" = none ] || { [ -z "$MISSING_REQ" ] && [ -z "$MISSING_OPT" ]; }; then
  [ -z "$MISSING_REQ" ]
  exit $?
fi

TARGETS="$MISSING_REQ $MISSING_OPT"
if [ "$MODE" = ask ]; then
  echo
  if [ -n "$MISSING_REQ" ] && [ -n "$MISSING_OPT" ]; then
    printf 'Install now? [a]ll / [r]equired only / [N]o: '
  else
    printf 'Install now? [y/N]: '
  fi
  read -r answer || answer=""
  case "$answer" in
    a | A | y | Y | yes | YES) ;;
    r | R) TARGETS="$MISSING_REQ" ;;
    *)
      echo "Nothing installed."
      [ -z "$MISSING_REQ" ]
      exit $?
      ;;
  esac
fi

echo
# shellcheck disable=SC2086
install_cmds $TARGETS >"$TMP/cmds"
# Read the commands on fd 3 so that sudo / brew keep the terminal on stdin.
while IFS= read -r cmd <&3; do
  echo "${B}\$ $cmd${N}"
  sh -c "$cmd" || echo "  (command failed; continuing)"
done 3<"$TMP/cmds"

if grep -q xcode-select "$TMP/cmds"; then
  echo
  echo "xcode-select opens an installer window; run this script again when it has finished."
fi

run_checks
print_report
[ -z "$MISSING_REQ" ]
