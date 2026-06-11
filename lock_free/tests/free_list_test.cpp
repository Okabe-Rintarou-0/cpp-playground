#include <gtest/gtest.h>
#include "../free_list.hpp"
#include <thread>
#include <vector>

namespace {

struct Watcher {
    static int count;
    Watcher() { ++count; }
    ~Watcher() { --count; }
};
int Watcher::count = 0;

TEST(FreeListTest, AllocateDeallocate) {
    FreeList<int, 1024> fl;
    int* a = fl.allocate(42);
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(*a, 42);

    int* b = fl.allocate(100);
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(*b, 100);

    fl.deallocate(a);
    fl.deallocate(b);
}

TEST(FreeListTest, ReuseFreedSlot) {
    FreeList<int, 1024> fl;
    int* a = fl.allocate(1);
    ASSERT_NE(a, nullptr);
    *a = 10;

    fl.deallocate(a);

    int* b = fl.allocate(2);
    EXPECT_EQ(b, a);
    EXPECT_EQ(*b, 2);
}

TEST(FreeListTest, AllocateUntilFull) {
    FreeList<int, 4> fl;
    std::vector<int*> ptrs;
    for (uint32_t i = 0; i < 4; ++i) {
        int* p = fl.allocate(static_cast<int>(i));
        ASSERT_NE(p, nullptr);
        EXPECT_EQ(*p, static_cast<int>(i));
        ptrs.push_back(p);
    }
    EXPECT_EQ(fl.allocate(0), nullptr);

    for (auto p : ptrs) {
        fl.deallocate(p);
    }
}

TEST(FreeListTest, ConstructorArgs) {
    struct Point {
        int x, y;
        Point(int a, int b) : x(a), y(b) {}
    };

    FreeList<Point, 16> fl;
    Point* p = fl.allocate(3, 4);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->x, 3);
    EXPECT_EQ(p->y, 4);
    fl.deallocate(p);
}

TEST(FreeListTest, DestroyObjectOnDeallocate) {
    EXPECT_EQ(Watcher::count, 0);
    {
        FreeList<Watcher, 8> fl;
        Watcher* w = fl.allocate();
        ASSERT_NE(w, nullptr);
        EXPECT_EQ(Watcher::count, 1);
        fl.deallocate(w);
        EXPECT_EQ(Watcher::count, 0);
    }
    EXPECT_EQ(Watcher::count, 0);
}

TEST(FreeListTest, DefaultCapacityIsCapacity) {
    FreeList<int, 64> fl;
    for (uint32_t i = 0; i < 64; ++i) {
        EXPECT_NE(fl.allocate(i), nullptr);
    }
    EXPECT_EQ(fl.allocate(0), nullptr);
}

TEST(FreeListTest, MultipleAllocateAndDeallocateCycles) {
    FreeList<int, 8> fl;
    for (int cycle = 0; cycle < 100; ++cycle) {
        int* ptrs[8];
        for (int i = 0; i < 8; ++i) {
            ptrs[i] = fl.allocate(i + cycle * 10);
            ASSERT_NE(ptrs[i], nullptr);
            EXPECT_EQ(*ptrs[i], i + cycle * 10);
        }
        EXPECT_EQ(fl.allocate(0), nullptr);
        for (int i = 0; i < 8; ++i) {
            fl.deallocate(ptrs[i]);
        }
    }
}

TEST(FreeListTest, ConcurrentAllocate) {
    FreeList<int, 1024> fl;
    std::atomic<int> sum{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&fl, &sum] {
            for (int i = 0; i < 128; ++i) {
                int* p = fl.allocate(1);
                if (p) {
                    sum.fetch_add(*p, std::memory_order_relaxed);
                }
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }
    EXPECT_EQ(sum.load(), 1024);
}

TEST(FreeListTest, ConcurrentAllocateDeallocate) {
    FreeList<int, 128> fl;
    std::atomic<long long> total{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&fl, &total] {
            for (int i = 0; i < 1000; ++i) {
                int* p = fl.allocate(i);
                if (p) {
                    total.fetch_add(*p, std::memory_order_relaxed);
                    fl.deallocate(p);
                }
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }
    (void)total;
}

TEST(FreeListTest, Alignment) {
    FreeList<int, 32> fl;
    int* p = fl.allocate(42);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ((reinterpret_cast<uintptr_t>(p) % FreeList<int, 32>::Alignment), 0);
    fl.deallocate(p);
}

} // namespace
