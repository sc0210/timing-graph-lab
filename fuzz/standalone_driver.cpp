// Runs a fuzz target without libFuzzer: replays every file of the given corpus
// directories, then N deterministic pseudo-random inputs. Used as a regular test so the
// fuzz targets stay exercised on every compiler, including GCC.
//
//   <target>_replay [--random N] [--seed S] CORPUS_DIR_OR_FILE...
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size);

namespace {
void run_file(const std::filesystem::path& p, std::size_t& count) {
  std::ifstream f(p, std::ios::binary);
  const std::vector<char> bytes{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
  LLVMFuzzerTestOneInput(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
  ++count;
}
}  // namespace

int main(int argc, char** argv) {
  std::size_t random_inputs = 0, files = 0;
  std::uint64_t seed = 1;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--random" && i + 1 < argc) {
      random_inputs = std::strtoull(argv[++i], nullptr, 10);
    } else if (a == "--seed" && i + 1 < argc) {
      seed = std::strtoull(argv[++i], nullptr, 10);
    } else if (std::filesystem::is_directory(a)) {
      for (const auto& e : std::filesystem::directory_iterator(a))
        if (e.is_regular_file()) run_file(e.path(), files);
    } else {
      run_file(a, files);
    }
  }
  // splitmix64 byte stream; input lengths 0..512.
  std::vector<std::uint8_t> buf;
  for (std::size_t k = 0; k < random_inputs; ++k) {
    auto next = [&seed] {
      std::uint64_t z = (seed += 0x9E3779B97F4A7C15ULL);
      z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
      z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
      return z ^ (z >> 31);
    };
    buf.resize(next() % 513);
    for (auto& b : buf) b = static_cast<std::uint8_t>(next());
    LLVMFuzzerTestOneInput(buf.data(), buf.size());
  }
  std::printf("replayed %zu corpus file(s) and %zu random input(s)\n", files, random_inputs);
  return 0;
}
