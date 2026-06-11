#include <new>
#include <cstddef>

template <typename T>
static constexpr std::size_t CacheAlignment = (alignof(T) > std::hardware_destructive_interference_size)
                                                  ? alignof(T)
                                                  : std::hardware_destructive_interference_size;