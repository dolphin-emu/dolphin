// SPDX-License-Identifier: CC0-1.0

#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <utility>

namespace Common
{

// An std::vector-like container that uses no heap allocations but is limited to a maximum size.
template <typename T, size_t MaxSize>
class SmallVector final
{
public:
  using value_type = T;

  SmallVector() = default;
  explicit SmallVector(size_t new_size) { resize(new_size); }

  ~SmallVector() { clear(); }

  SmallVector(const SmallVector& other)
  {
    for (auto& value : other)
      emplace_back(value);
  }

  SmallVector& operator=(const SmallVector& rhs)
  {
    clear();
    for (auto& value : rhs)
      emplace_back(value);
    return *this;
  }

  SmallVector(SmallVector&& other)
  {
    for (auto& value : other)
      emplace_back(std::move(value));
    other.clear();
  }

  SmallVector& operator=(SmallVector&& rhs)
  {
    clear();
    for (auto& value : rhs)
      emplace_back(std::move(value));
    rhs.clear();
    return *this;
  }

  void push_back(const value_type& x) { emplace_back(x); }
  void push_back(value_type&& x) { emplace_back(std::move(x)); }

  template <typename... Args>
  value_type& emplace_back(Args&&... args)
  {
    assert(m_size < MaxSize);
    return *::new (&m_array[m_size++ * sizeof(value_type)]) value_type{std::forward<Args>(args)...};
  }

  void pop_back()
  {
    assert(m_size > 0);
    std::destroy_at(data() + --m_size);
  }

  value_type& operator[](size_t i)
  {
    assert(i < m_size);
    return data()[i];
  }
  const value_type& operator[](size_t i) const
  {
    assert(i < m_size);
    return data()[i];
  }

  auto data() { return std::launder(reinterpret_cast<value_type*>(m_array.data())); }
  auto begin() { return data(); }
  auto end() { return data() + m_size; }

  auto data() const { return std::launder(reinterpret_cast<const value_type*>(m_array.data())); }
  auto begin() const { return data(); }
  auto end() const { return data() + m_size; }

  size_t capacity() const { return MaxSize; }
  size_t size() const { return m_size; }

  bool empty() const { return m_size == 0; }

  void resize(size_t new_size)
  {
    assert(new_size <= MaxSize);

    while (size() < new_size)
      emplace_back();

    while (size() > new_size)
      pop_back();
  }

  void clear() { resize(0); }

private:
  alignas(value_type) std::array<std::byte, MaxSize * sizeof(value_type)> m_array;
  size_t m_size = 0;
};

}  // namespace Common
