// SPDX-License-Identifier: CC0-1.0

#pragma once

#include <array>
#include <cstddef>
#include <utility>

namespace Common
{

// An std::vector-like container that uses no heap allocations but is limited to a maximum size.
template <typename T, size_t MaxSize>
class SmallVector final
{
public:
  SmallVector() = default;
  explicit SmallVector(size_t size) : m_size(size) {}

  void push_back(const T& x) { m_array[m_size++] = x; }
  void push_back(T&& x) { m_array[m_size++] = std::move(x); }

  template <typename... Args>
  T& emplace_back(Args&&... args)
  {
    return m_array[m_size++] = T{std::forward<Args>(args)...};
  }

  T& operator[](size_t i) { return m_array[i]; }
  const T& operator[](size_t i) const { return m_array[i]; }

  auto data() { return m_array.data(); }
  auto begin() { return m_array.begin(); }
  auto end() { return m_array.begin() + m_size; }

  auto data() const { return m_array.data(); }
  auto begin() const { return m_array.begin(); }
  auto end() const { return m_array.begin() + m_size; }

  size_t size() const { return m_size; }
  bool empty() const { return m_size == 0; }

private:
  std::array<T, MaxSize> m_array{};
  size_t m_size = 0;
};

}  // namespace Common
