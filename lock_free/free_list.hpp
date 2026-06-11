#include "align.hpp"
#include <atomic>
#include <cstdint>
#include <utility>

class TaggedIndex {
public:
  TaggedIndex() = default;
  TaggedIndex(uint32_t tag, uint32_t index) : tag_(tag), index_(index) {}

  uint32_t next_tag() const { return tag_ + 1; }

  uint32_t get_index() const { return index_; }

private:
  uint32_t tag_;
  uint32_t index_;
};

template <typename T, uint32_t Capacity> class FreeList {

public:
  static constexpr size_t Alignment = CacheAlignment<T>;

  union alignas(Alignment) Node {
    T data;
    TaggedIndex next;

    Node() {}
    ~Node() {}
  };
  FreeList() {
    for (uint32_t i = 0; i < Capacity - 1; ++i) {
      nodes_[i].next = TaggedIndex(0, i + 1);
    }
    nodes_[Capacity - 1].next = TaggedIndex(0, Capacity);
    head_.store(TaggedIndex(0, 0), std::memory_order_relaxed);
  }

  template <typename... Args> T *allocate(Args &&...args) {
    TaggedIndex old_head = head_.load(std::memory_order_acquire);
    TaggedIndex new_head;
    uint32_t index;
    do {
      index = old_head.get_index();
      if (index == Capacity) {
        return nullptr;
      }
      uint32_t next_idx = nodes_[index].next.get_index();
      new_head = TaggedIndex(old_head.next_tag(), next_idx);
    } while (!head_.compare_exchange_strong(old_head, new_head,
                                            std::memory_order_release,
                                            std::memory_order_acquire));
    T *ptr = &nodes_[index].data;
    ::new (ptr) T(std::forward<Args>(args)...);
    return ptr;
  }

  void deallocate(T *ptr) {
    ptr->~T();
    uint32_t index =
        static_cast<uint32_t>(reinterpret_cast<Node *>(ptr) - nodes_);
    TaggedIndex old_head = head_.load(std::memory_order_acquire);
    TaggedIndex new_head;
    do {
      nodes_[index].next = TaggedIndex(0, old_head.get_index());
      new_head = TaggedIndex(old_head.next_tag(), index);
    } while (!head_.compare_exchange_strong(old_head, new_head,
                                            std::memory_order_release,
                                            std::memory_order_acquire));
  }

private:
  Node nodes_[Capacity];
  alignas(Alignment) std::atomic<TaggedIndex> head_;
};