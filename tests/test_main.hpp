#pragma once
// Minimal dependency-free test harness: TEST(name) { CHECK(...); } and a main().
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace tgl::test {

struct Case {
  const char* name;
  std::function<void()> fn;
};
inline std::vector<Case>& registry() {
  static std::vector<Case> r;
  return r;
}
inline int& failures() {
  static int f = 0;
  return f;
}
struct Registrar {
  Registrar(const char* n, std::function<void()> f) { registry().push_back({n, std::move(f)}); }
};

}  // namespace tgl::test

#define TGL_CAT2(a, b) a##b
#define TGL_CAT(a, b) TGL_CAT2(a, b)
#define TEST(name)                                                                       \
  static void name();                                                                    \
  static ::tgl::test::Registrar TGL_CAT(reg_, name)(#name, name);                        \
  static void name()

#define CHECK(cond)                                                                      \
  do {                                                                                   \
    if (!(cond)) {                                                                       \
      std::fprintf(stderr, "  %s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);    \
      ++::tgl::test::failures();                                                         \
    }                                                                                    \
  } while (0)

#define CHECK_EQ(a, b)                                                                   \
  do {                                                                                   \
    const auto va_ = (a);                                                                \
    const auto vb_ = (b);                                                                \
    if (!(va_ == vb_)) {                                                                 \
      std::fprintf(stderr, "  %s:%d: CHECK_EQ failed: %s == %s\n", __FILE__, __LINE__, #a, #b); \
      ++::tgl::test::failures();                                                         \
    }                                                                                    \
  } while (0)

int main(int argc, char** argv) {
  const std::string filter = argc > 1 ? argv[1] : "";
  int run = 0;
  for (auto& c : ::tgl::test::registry()) {
    if (!filter.empty() && std::string(c.name).find(filter) == std::string::npos) continue;
    const int before = ::tgl::test::failures();
    c.fn();
    ++run;
    std::printf("[%s] %s\n", ::tgl::test::failures() == before ? " OK " : "FAIL", c.name);
  }
  std::printf("%d tests, %d check failures\n", run, ::tgl::test::failures());
  return ::tgl::test::failures() == 0 ? 0 : 1;
}
