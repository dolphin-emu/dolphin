// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

// UniqueBuffer<T> and SharedBuffer<T> are a lighter alternative to std::vector<T>.
// The main benefit is that elements are not value-initialized like in vector.
// That can be quite a bit of unecessary overhead when allocating a large buffer.

namespace Common
{

namespace detail
{
template <auto MakeFunc, typename T>
class BufferBase final
{
public:
  using PtrType = decltype(MakeFunc(1));

  using value_type = T;
  using size_type = std::size_t;

  BufferBase() {}
  BufferBase(size_type size) : m_ptr{MakeFunc(size)}, m_size{size} {}

  void swap(BufferBase& other)
  {
    std::swap(m_ptr, other.m_ptr);
    std::swap(m_size, other.m_size);
  }

  size_type size() const { return m_size; }
  bool empty() const { return m_size == 0; }

  T* get() { return m_ptr.get(); }
  const T* get() const { return m_ptr.get(); }

  T* data() { return m_ptr.get(); }
  const T* data() const { return m_ptr.get(); }

  T* begin() { return m_ptr.get(); }
  T* end() { return m_ptr.get() + m_size; }

  const T* begin() const { return m_ptr.get(); }
  const T* end() const { return m_ptr.get() + m_size; }

  T& operator[](size_type index) { return m_ptr[index]; }
  const T& operator[](size_type index) const { return m_ptr[index]; }

private:
  PtrType m_ptr;
  size_type m_size = 0;
};
}  // namespace detail

template <typename T>
using UniqueBuffer = detail::BufferBase<std::make_unique_for_overwrite<T[]>, T>;

// TODO: std::make_shared_for_overwrite requires GCC 12.1+
// template <typename T>
// using SharedBuffer = detail::BufferBase<std::make_shared_for_overwrite<T[]>, T>;

}  // namespace Common
