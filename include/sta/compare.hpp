#pragma once
// Numeric comparison of timing results (plan §7.4).
//   close(x, y)  <=>  x == y  or  |x - y| <= eps_abs + eps_rel * max(|x|, |y|)
// Defaults eps_abs = eps_rel = 1e-9 are a starting point for this lab, not an STA
// accuracy standard. Bitwise equality is reported separately and is not required.
// Any NaN, or any +-inf in a result, counts as a failure (every node of a DAG has
// finite AT and RT, so an infinity indicates an uninitialized or unreachable value).
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "sta/timing_state.hpp"

namespace tgl {

struct Tolerance {
  double abs = 1e-9;
  double rel = 1e-9;
};

struct CompareResult {
  bool pass = true;
  bool bitwise = true;
  std::size_t mismatches = 0;
  std::size_t non_finite = 0;
  double max_abs_err = 0;

  void merge(const CompareResult& o) {
    pass = pass && o.pass;
    bitwise = bitwise && o.bitwise;
    mismatches += o.mismatches;
    non_finite += o.non_finite;
    max_abs_err = std::max(max_abs_err, o.max_abs_err);
  }
};

inline bool close(double x, double y, const Tolerance& t) {
  if (x == y) return true;
  if (!std::isfinite(x) || !std::isfinite(y)) return false;
  return std::fabs(x - y) <= t.abs + t.rel * std::max(std::fabs(x), std::fabs(y));
}

inline CompareResult compare_values(const std::vector<Delay>& got, const std::vector<Delay>& ref,
                                    const Tolerance& t = {}) {
  CompareResult r;
  if (got.size() != ref.size()) {
    r.pass = r.bitwise = false;
    r.mismatches = std::max(got.size(), ref.size());
    return r;
  }
  r.bitwise = got.empty() || std::memcmp(got.data(), ref.data(), got.size() * sizeof(Delay)) == 0;
  for (std::size_t i = 0; i < got.size(); ++i) {
    if (!std::isfinite(got[i])) ++r.non_finite;
    if (!close(got[i], ref[i], t)) ++r.mismatches;
    if (std::isfinite(got[i]) && std::isfinite(ref[i]))
      r.max_abs_err = std::max(r.max_abs_err, std::fabs(got[i] - ref[i]));
  }
  r.pass = r.mismatches == 0 && r.non_finite == 0;
  return r;
}

inline CompareResult compare_timing(const TimingState& got, const TimingState& ref, const Tolerance& t = {}) {
  CompareResult r = compare_values(got.at, ref.at, t);
  r.merge(compare_values(got.rt, ref.rt, t));
  r.merge(compare_values(got.slack, ref.slack, t));
  return r;
}

}  // namespace tgl
