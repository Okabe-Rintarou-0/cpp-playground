#include "free_list.hpp"
#include <atomic>
#include <concepts>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

template <typename T> class alignas(16) TaggedPtr {
public:
  TaggedPtr() noexcept : ptr_(nullptr), tag_(0) {}
  TaggedPtr(T *ptr, uint64_t tag) noexcept : ptr_(ptr), tag_(tag) {}

  T *get_ptr() const noexcept { return ptr_; }
  uint64_t next_tag() const noexcept { return tag_ + 1; }

  friend bool operator==(const TaggedPtr &, const TaggedPtr &) = default;

private:
  T *ptr_;
  uint64_t tag_;
};

template <typename T, uint32_t Capacity>
  requires std::destructible<T> && (Capacity > 0)
class LockFreeStack {
public:
  LockFreeStack() : free_list_(std::make_unique<FreeList<Node, Capacity>>()) {}

  template <typename... Args> void emplace(Args &&...args) {
    Node *node = free_list_->allocate();
    if (!node)
      return;
    ::new (&node->data) T(std::forward<Args>(args)...);
    push_node(node);
  }

  void push(T &&value) {
    Node *node = free_list_->allocate();
    if (!node)
      return;
    node->data = std::move(value);
    push_node(node);
  }

  std::optional<T> pop() {
    TaggedPtr<Node> old_head = head_.load(std::memory_order_acquire);
    TaggedPtr<Node> new_head;
    do {
      if (!old_head.get_ptr())
        return std::nullopt;
      new_head = TaggedPtr<Node>(old_head.get_ptr()->next.get_ptr(),
                                 old_head.next_tag());
    } while (!head_.compare_exchange_strong(old_head, new_head,
                                            std::memory_order_release,
                                            std::memory_order_acquire));
    std::optional<T> out(std::move(old_head.get_ptr()->data));
    free_list_->deallocate(old_head.get_ptr());
    return out;
  }

private:
  struct Node {
    T data;
    TaggedPtr<Node> next;
  };

  void push_node(Node *node) {
    TaggedPtr<Node> old_head = head_.load(std::memory_order_relaxed);
    TaggedPtr<Node> new_head;
    do {
      node->next = old_head;
      new_head = TaggedPtr<Node>(node, old_head.next_tag());
    } while (!head_.compare_exchange_strong(old_head, new_head,
                                            std::memory_order_release,
                                            std::memory_order_acquire));
  }

  std::unique_ptr<FreeList<Node, Capacity>> free_list_;
  alignas(16) std::atomic<TaggedPtr<Node>> head_{TaggedPtr<Node>(nullptr, 0)};
};
