#include "lock_free_stack.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <stack>
#include <thread>
#include <vector>

// ---- type sizes under test ----
struct alignas(64) Type64 {
  char data[64];
};

struct alignas(256) Type256 {
  char data[256];
};

// ---- mutex-based stack for comparison ----
template <typename T> class MutexStack {
public:
  void push(const T &value) {
    std::lock_guard lock(m_);
    stack_.push(value);
  }

  std::optional<T> pop() {
    std::lock_guard lock(m_);
    if (stack_.empty())
      return std::nullopt;
    T v = std::move(stack_.top());
    stack_.pop();
    return v;
  }

private:
  std::mutex m_;
  std::stack<T> stack_;
};

// ---- benchmark result ----
struct Result {
  const char *type_name;
  const char *workload;
  int threads;
  double lf_ops;
  double mx_ops;
};

// ---- workload: push-only ----
template <typename T, uint32_t Capacity = 4096>
static double bench_lockfree_push(int threads, int ops_per_thread) {
  LockFreeStack<T, Capacity> stack;
  std::vector<std::thread> threads_vec;
  auto start = std::chrono::steady_clock::now();
  for (int t = 0; t < threads; ++t) {
    threads_vec.emplace_back([&] {
      for (int i = 0; i < ops_per_thread; ++i)
        stack.push(T());
    });
  }
  for (auto &th : threads_vec)
    th.join();
  auto end = std::chrono::steady_clock::now();
  double sec = std::chrono::duration<double>(end - start).count();
  return static_cast<double>(threads) * ops_per_thread / sec;
}

template <typename T>
static double bench_mutex_push(int threads, int ops_per_thread) {
  MutexStack<T> stack;
  std::vector<std::thread> threads_vec;
  auto start = std::chrono::steady_clock::now();
  for (int t = 0; t < threads; ++t) {
    threads_vec.emplace_back([&] {
      for (int i = 0; i < ops_per_thread; ++i)
        stack.push(T());
    });
  }
  for (auto &th : threads_vec)
    th.join();
  auto end = std::chrono::steady_clock::now();
  double sec = std::chrono::duration<double>(end - start).count();
  return static_cast<double>(threads) * ops_per_thread / sec;
}

// ---- workload: pop-only (stack pre-filled) ----
template <typename T, uint32_t Capacity = 4096>
static double bench_lockfree_pop(int threads, int ops_per_thread) {
  LockFreeStack<T, Capacity> stack;
  // pre-fill
  for (int i = 0; i < Capacity; ++i)
    stack.push(T());
  std::vector<std::thread> threads_vec;
  auto start = std::chrono::steady_clock::now();
  for (int t = 0; t < threads; ++t) {
    threads_vec.emplace_back([&] {
      for (int i = 0; i < ops_per_thread; ++i)
        stack.pop();
    });
  }
  for (auto &th : threads_vec)
    th.join();
  auto end = std::chrono::steady_clock::now();
  double sec = std::chrono::duration<double>(end - start).count();
  return static_cast<double>(threads) * ops_per_thread / sec;
}

template <typename T>
static double bench_mutex_pop(int threads, int ops_per_thread) {
  MutexStack<T> stack;
  for (int i = 0; i < 4096; ++i)
    stack.push(T());
  std::vector<std::thread> threads_vec;
  auto start = std::chrono::steady_clock::now();
  for (int t = 0; t < threads; ++t) {
    threads_vec.emplace_back([&] {
      for (int i = 0; i < ops_per_thread; ++i)
        stack.pop();
    });
  }
  for (auto &th : threads_vec)
    th.join();
  auto end = std::chrono::steady_clock::now();
  double sec = std::chrono::duration<double>(end - start).count();
  return static_cast<double>(threads) * ops_per_thread / sec;
}

// ---- workload: push+pop balanced (each thread does pair) ----
template <typename T, uint32_t Capacity = 4096>
static double bench_lockfree_mixed(int threads, int ops_per_thread) {
  LockFreeStack<T, Capacity> stack;
  std::vector<std::thread> threads_vec;
  auto start = std::chrono::steady_clock::now();
  for (int t = 0; t < threads; ++t) {
    threads_vec.emplace_back([&] {
      for (int i = 0; i < ops_per_thread; ++i) {
        stack.push(T());
        stack.pop();
      }
    });
  }
  for (auto &th : threads_vec)
    th.join();
  auto end = std::chrono::steady_clock::now();
  double sec = std::chrono::duration<double>(end - start).count();
  return static_cast<double>(threads) * ops_per_thread / sec;
}

template <typename T>
static double bench_mutex_mixed(int threads, int ops_per_thread) {
  MutexStack<T> stack;
  std::vector<std::thread> threads_vec;
  auto start = std::chrono::steady_clock::now();
  for (int t = 0; t < threads; ++t) {
    threads_vec.emplace_back([&] {
      for (int i = 0; i < ops_per_thread; ++i) {
        stack.push(T());
        stack.pop();
      }
    });
  }
  for (auto &th : threads_vec)
    th.join();
  auto end = std::chrono::steady_clock::now();
  double sec = std::chrono::duration<double>(end - start).count();
  return static_cast<double>(threads) * ops_per_thread / sec;
}

// ---- ops per thread lookup (scaled to keep runtime reasonable) ----
static int ops_for_threads(int threads) {
  switch (threads) {
  case 1:
    return 2000000;
  case 2:
    return 1000000;
  case 4:
    return 500000;
  case 8:
    return 250000;
  default:
    return 500000;
  }
}

int main() {
  const int thread_counts[] = {1, 2, 4, 8};
  const char *workloads[] = {"push", "mixed", "pop"};
  const char *type_names[] = {"int", "Type64", "Type256"};
  std::vector<Result> results;

  for (auto tn : type_names) {
    for (int ti = 0; ti < 4; ++ti) {
      int thr = thread_counts[ti];
      int ops = ops_for_threads(thr);

      for (auto wl : workloads) {
        Result r;
        r.type_name = tn;
        r.workload = wl;
        r.threads = thr;

        bool is_int = std::strcmp(tn, "int") == 0;
        bool is_64 = std::strcmp(tn, "Type64") == 0;

        // lock-free
        if (std::strcmp(wl, "push") == 0) {
          r.lf_ops =
              is_int ? bench_lockfree_push<int>(thr, ops)
              : is_64 ? bench_lockfree_push<Type64>(thr, ops)
                      : bench_lockfree_push<Type256>(thr, ops);
        } else if (std::strcmp(wl, "pop") == 0) {
          r.lf_ops =
              is_int ? bench_lockfree_pop<int>(thr, ops)
              : is_64 ? bench_lockfree_pop<Type64>(thr, ops)
                      : bench_lockfree_pop<Type256>(thr, ops);
        } else {
          r.lf_ops =
              is_int ? bench_lockfree_mixed<int>(thr, ops)
              : is_64 ? bench_lockfree_mixed<Type64>(thr, ops)
                      : bench_lockfree_mixed<Type256>(thr, ops);
        }

        // mutex
        if (std::strcmp(wl, "push") == 0) {
          r.mx_ops =
              is_int ? bench_mutex_push<int>(thr, ops)
              : is_64 ? bench_mutex_push<Type64>(thr, ops)
                      : bench_mutex_push<Type256>(thr, ops);
        } else if (std::strcmp(wl, "pop") == 0) {
          r.mx_ops =
              is_int ? bench_mutex_pop<int>(thr, ops)
              : is_64 ? bench_mutex_pop<Type64>(thr, ops)
                      : bench_mutex_pop<Type256>(thr, ops);
        } else {
          r.mx_ops =
              is_int ? bench_mutex_mixed<int>(thr, ops)
              : is_64 ? bench_mutex_mixed<Type64>(thr, ops)
                      : bench_mutex_mixed<Type256>(thr, ops);
        }

        results.push_back(r);
      }
    }
  }

  // ---- CSV output ----
  std::ofstream csv("bench_results.csv");
  csv << "type,threads,workload,lockfree_ops,mutex_ops,speedup\n";
  for (auto &r : results) {
    csv << r.type_name << "," << r.threads << "," << r.workload << ","
        << r.lf_ops << "," << r.mx_ops << ","
        << (r.mx_ops > 0 ? r.lf_ops / r.mx_ops : 0) << "\n";
  }
  csv.close();
  std::printf("Results saved to bench_results.csv\n");

  // ---- console output ----
  std::printf("\n%-8s | %-6s | %-8s | %14s | %14s | %7s\n", "type",
              "threads", "workload", "lockfree", "mutex", "speedup");
  std::printf("%s\n", std::string(80, '-').c_str());
  for (auto &r : results) {
    auto fmt = [](double v, char *buf) {
      if (v >= 1e6)
        std::sprintf(buf, "%.2f M", v / 1e6);
      else if (v >= 1e3)
        std::sprintf(buf, "%.2f K", v / 1e3);
      else
        std::sprintf(buf, "%.0f", v);
    };
    char lf_buf[16], mx_buf[16];
    fmt(r.lf_ops, lf_buf);
    fmt(r.mx_ops, mx_buf);
    std::printf("%-8s | %-6d | %-8s | %12s | %12s | %6.2fx\n", r.type_name,
                r.threads, r.workload, lf_buf, mx_buf,
                r.mx_ops > 0 ? r.lf_ops / r.mx_ops : 0);
  }

  return 0;
}
