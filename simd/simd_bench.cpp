// Bypass target feature checks that clang-cl doesn't define with /arch:AVX2
#define HWY_DISABLE_BMI2_FMA
#define HWY_DISABLE_F16C
#define HWY_DISABLE_PCLMUL_AES

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <vector>

#include "hwy/highway.h"
#include "hwy/aligned_allocator.h"

namespace hn = hwy::HWY_NAMESPACE;

// ---- Implementation approaches ----

static void scalar_plain(double* v, size_t n, double fill) {
  for (size_t i = 0; i < n; ++i)
    v[i] = std::max(v[i] - fill, 0.0);
}

static void scalar_unrolled4(double* v, size_t n, double fill) {
  size_t i = 0;
  for (; i + 4 <= n; i += 4) {
    v[i]     = std::max(v[i]     - fill, 0.0);
    v[i + 1] = std::max(v[i + 1] - fill, 0.0);
    v[i + 2] = std::max(v[i + 2] - fill, 0.0);
    v[i + 3] = std::max(v[i + 3] - fill, 0.0);
  }
  for (; i < n; ++i)
    v[i] = std::max(v[i] - fill, 0.0);
}

static void scalar_unrolled8(double* v, size_t n, double fill) {
  size_t i = 0;
  for (; i + 8 <= n; i += 8) {
    v[i]     = std::max(v[i]     - fill, 0.0);
    v[i + 1] = std::max(v[i + 1] - fill, 0.0);
    v[i + 2] = std::max(v[i + 2] - fill, 0.0);
    v[i + 3] = std::max(v[i + 3] - fill, 0.0);
    v[i + 4] = std::max(v[i + 4] - fill, 0.0);
    v[i + 5] = std::max(v[i + 5] - fill, 0.0);
    v[i + 6] = std::max(v[i + 6] - fill, 0.0);
    v[i + 7] = std::max(v[i + 7] - fill, 0.0);
  }
  for (; i < n; ++i)
    v[i] = std::max(v[i] - fill, 0.0);
}

static void simd_impl(double* v, size_t n, double fill) {
  const hn::ScalableTag<double> d;
  const auto fill_v = hn::Set(d, fill);
  const auto zero   = hn::Zero(d);
  size_t i = 0;
  for (; i + hn::Lanes(d) <= n; i += hn::Lanes(d)) {
    auto x = hn::LoadU(d, v + i);
    x = hn::Max(hn::Sub(x, fill_v), zero);
    hn::StoreU(x, d, v + i);
  }
  for (; i < n; ++i)
    v[i] = std::max(v[i] - fill, 0.0);
}

static void simd_unrolled2(double* v, size_t n, double fill) {
  const hn::ScalableTag<double> d;
  const auto fill_v = hn::Set(d, fill);
  const auto zero   = hn::Zero(d);
  const size_t N = hn::Lanes(d);
  size_t i = 0;
  for (; i + 2 * N <= n; i += 2 * N) {
    auto x0 = hn::LoadU(d, v + i);
    auto x1 = hn::LoadU(d, v + i + N);
    x0 = hn::Max(hn::Sub(x0, fill_v), zero);
    x1 = hn::Max(hn::Sub(x1, fill_v), zero);
    hn::StoreU(x0, d, v + i);
    hn::StoreU(x1, d, v + i + N);
  }
  for (; i < n; ++i)
    v[i] = std::max(v[i] - fill, 0.0);
}

static void simd_unrolled4(double* v, size_t n, double fill) {
  const hn::ScalableTag<double> d;
  const auto fill_v = hn::Set(d, fill);
  const auto zero   = hn::Zero(d);
  const size_t N = hn::Lanes(d);
  size_t i = 0;
  for (; i + 4 * N <= n; i += 4 * N) {
    auto x0 = hn::LoadU(d, v + i);
    auto x1 = hn::LoadU(d, v + i + N);
    auto x2 = hn::LoadU(d, v + i + 2 * N);
    auto x3 = hn::LoadU(d, v + i + 3 * N);
    x0 = hn::Max(hn::Sub(x0, fill_v), zero);
    x1 = hn::Max(hn::Sub(x1, fill_v), zero);
    x2 = hn::Max(hn::Sub(x2, fill_v), zero);
    x3 = hn::Max(hn::Sub(x3, fill_v), zero);
    hn::StoreU(x0, d, v + i);
    hn::StoreU(x1, d, v + i + N);
    hn::StoreU(x2, d, v + i + 2 * N);
    hn::StoreU(x3, d, v + i + 3 * N);
  }
  for (; i < n; ++i)
    v[i] = std::max(v[i] - fill, 0.0);
}

// ---- Benchmark types ----
struct Result {
  const char* approach;
  size_t vec_size;
  double throughput_gbs;
};

struct Approach {
  const char* name;
  void (*func)(double*, size_t, double);
};

// ---- Test parameters ----
static constexpr size_t kSizes[] = {
  8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096
};
static constexpr int kNumSizes = sizeof(kSizes) / sizeof(kSizes[0]);

static int iterations_for_size(size_t n) {
  if (n <= 8)    return 10000000;
  if (n <= 16)   return  8000000;
  if (n <= 32)   return  4000000;
  if (n <= 64)   return  2000000;
  if (n <= 128)  return  1000000;
  if (n <= 256)  return   500000;
  if (n <= 512)  return   200000;
  if (n <= 1024) return   100000;
  if (n <= 2048) return    50000;
  return                  25000;
}

// ---- Helpers ----
static bool verify_equal(const double* a, const double* b, size_t n) {
  for (size_t i = 0; i < n; ++i)
    if (std::abs(a[i] - b[i]) > 1e-12) return false;
  return true;
}

static double run_benchmark(const Approach& app,
                            const std::vector<double, hwy::AlignedAllocator<double>>& src,
                            double fill, int iterations) {
  using Alloc = hwy::AlignedAllocator<double>;
  std::vector<double, Alloc> buf(src.size());

  for (int i = 0; i < 3; ++i) {
    std::memcpy(buf.data(), src.data(), src.size() * sizeof(double));
    app.func(buf.data(), buf.size(), fill);
  }

  auto start = std::chrono::steady_clock::now();
  for (int iter = 0; iter < iterations; ++iter) {
    std::memcpy(buf.data(), src.data(), src.size() * sizeof(double));
    app.func(buf.data(), buf.size(), fill);
  }
  auto end = std::chrono::steady_clock::now();

  double sec = std::chrono::duration<double>(end - start).count();
  double total_bytes = static_cast<double>(iterations) * src.size() * sizeof(double) * 2.0;
  return total_bytes / (sec * 1e9);
}

// ---- Main ----
int main() {
  std::mt19937_64 rng(42);
  std::uniform_real_distribution<double> dist(0.0, 1000.0);

  Approach approaches[] = {
    {"scalar",        scalar_plain},
    {"unrolled4",     scalar_unrolled4},
    {"unrolled8",     scalar_unrolled8},
    {"simd",          simd_impl},
    {"simd_unroll2",  simd_unrolled2},
    {"simd_unroll4",  simd_unrolled4},
  };
  constexpr int kNumApproaches = sizeof(approaches) / sizeof(approaches[0]);

  std::vector<Result> results;
  using Alloc = hwy::AlignedAllocator<double>;

  std::printf("Benchmarking fill-sub-max on double vectors...\n");
  std::printf("HWY_LANES(double) = %zu\n", hn::Lanes(hn::ScalableTag<double>()));

  for (int si = 0; si < kNumSizes; ++si) {
    size_t n = kSizes[si];
    int iters = iterations_for_size(n);

    std::vector<double, Alloc> src(n);
    for (auto& v : src) v = dist(rng);
    double fill = dist(rng);

    std::vector<double, Alloc> ref(n);
    std::memcpy(ref.data(), src.data(), n * sizeof(double));
    scalar_plain(ref.data(), n, fill);

    for (int ai = 0; ai < kNumApproaches; ++ai) {
      std::vector<double, Alloc> buf(n);
      std::memcpy(buf.data(), src.data(), n * sizeof(double));
      approaches[ai].func(buf.data(), n, fill);
      if (!verify_equal(buf.data(), ref.data(), n)) {
        std::fprintf(stderr, "ERROR: %s differs at size %zu\n",
                     approaches[ai].name, n);
        return 1;
      }
    }

    for (int ai = 0; ai < kNumApproaches; ++ai) {
      double gbs = run_benchmark(approaches[ai], src, fill, iters);
      results.push_back({approaches[ai].name, n, gbs});
    }

    std::printf("  size %4zu: %d iters\n", n, iters);
  }

#ifdef NO_OPT_BUILD
  const char* csv_name = "../bench_results_simd_noopt.csv";
#else
  const char* csv_name = "../bench_results_simd.csv";
#endif
  std::ofstream csv(csv_name);
  csv << "approach,vec_size,throughput_gbs\n";
  for (auto& r : results)
    csv << r.approach << "," << r.vec_size << "," << r.throughput_gbs << "\n";
  csv.close();
  std::printf("Results saved to %s\n", csv_name);

  std::printf("\n%-12s | %10s | %16s\n", "approach", "vec_size", "throughput (GB/s)");
  std::printf("%s\n", std::string(55, '-').c_str());
  for (auto& r : results)
    std::printf("%-12s | %10zu | %14.2f\n", r.approach, r.vec_size, r.throughput_gbs);

  return 0;
}
