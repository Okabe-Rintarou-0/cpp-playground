#include "../lock_free_stack.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace {

TEST(LockFreeStackTest, PushPop) {
  LockFreeStack<int, 1024> stack;
  stack.push(42);
  auto val = stack.pop();
  EXPECT_TRUE(val.has_value());
  EXPECT_EQ(val.value(), 42);
}

TEST(LockFreeStackTest, PopEmpty) {
  LockFreeStack<int, 1024> stack;
  EXPECT_FALSE(stack.pop().has_value());
}

TEST(LockFreeStackTest, Emplace) {
  LockFreeStack<std::pair<int, int>, 1024> stack;
  stack.emplace(1, 2);
  auto val = stack.pop();
  ASSERT_TRUE(val.has_value());
  EXPECT_EQ(val->first, 1);
  EXPECT_EQ(val->second, 2);
}

TEST(LockFreeStackTest, LIFOOrder) {
  LockFreeStack<int, 1024> stack;
  stack.push(1);
  stack.push(2);
  stack.push(3);

  EXPECT_EQ(stack.pop(), 3);
  EXPECT_EQ(stack.pop(), 2);
  EXPECT_EQ(stack.pop(), 1);
  EXPECT_FALSE(stack.pop().has_value());
}

TEST(LockFreeStackTest, PushRvalue) {
  LockFreeStack<std::string, 128> stack;
  std::string s("hello");
  stack.push(std::move(s));
  EXPECT_TRUE(s.empty());

  auto val = stack.pop();
  ASSERT_TRUE(val.has_value());
  EXPECT_EQ(*val, "hello");
}

TEST(LockFreeStackTest, FillToCapacity) {
  LockFreeStack<int, 16> stack;
  for (uint32_t i = 0; i < 16; ++i) {
    stack.push(static_cast<int>(i));
  }
  // Capacity exhausted; push should silently no-op (FreeList returns nullptr)
  stack.push(999);

  int count = 0;
  while (stack.pop().has_value()) {
    ++count;
  }
  EXPECT_EQ(count, 16);
}

TEST(LockFreeStackTest, ConcurrentPushPop) {
  LockFreeStack<int, 4096> stack;
  std::atomic<long long> sum{0};
  constexpr int kItems = 512;
  constexpr int kThreads = 4;

  // Phase 1: concurrent pushes
  {
    std::vector<std::thread> pushers;
    for (int t = 0; t < kThreads; ++t) {
      pushers.emplace_back([&stack, t] {
        for (int i = 0; i < kItems; ++i) {
          stack.push(t * kItems + i);
        }
      });
    }
    for (auto &th : pushers)
      th.join();
  }

  // Phase 2: concurrent pops
  {
    std::vector<std::thread> poppers;
    for (int t = 0; t < kThreads; ++t) {
      poppers.emplace_back([&stack, &sum] {
        for (int i = 0; i < kItems; ++i) {
          auto v = stack.pop();
          if (v.has_value()) {
            sum.fetch_add(*v, std::memory_order_relaxed);
          }
        }
      });
    }
    for (auto &th : poppers)
      th.join();
  }

  // All 4*512 = 2048 items should have been popped
  EXPECT_EQ(sum.load(), 2048LL * 2047 / 2); // sum of 0..2047
  EXPECT_FALSE(stack.pop().has_value());
}

TEST(LockFreeStackTest, ConcurrentMixedWork) {
  LockFreeStack<int, 256> stack;
  std::atomic<int> pushed{0};
  std::atomic<int> popped{0};
  std::vector<std::thread> threads;

  for (int t = 0; t < 4; ++t) {
    threads.emplace_back([&] {
      for (int i = 0; i < 500; ++i) {
        if (i % 2 == 0) {
          stack.push(1);
          pushed.fetch_add(1, std::memory_order_relaxed);
        } else {
          if (stack.pop().has_value()) {
            popped.fetch_add(1, std::memory_order_relaxed);
          }
        }
      }
    });
  }
  for (auto &th : threads)
    th.join();

  // Stack should eventually empty or at worst be consistent
  int net = pushed.load() - popped.load();
  int remaining = 0;
  while (stack.pop().has_value()) {
    ++remaining;
  }
  EXPECT_EQ(remaining, net);
}

} // namespace
