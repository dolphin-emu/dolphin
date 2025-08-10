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
template <auto MakeFunc>
class BufferBase final
{
public:
  using PtrType = decltype(MakeFunc(1));

  using value_type = PtrType::element_type;
  using size_type = std::size_t;

  BufferBase() {}
  BufferBase(PtrType ptr, size_type new_size) : m_ptr{std::move(ptr)}, m_size{new_size} {}
  explicit BufferBase(size_type new_size) : BufferBase{MakeFunc(new_size), new_size} {}

  BufferBase(const BufferBase&) = default;
  BufferBase& operator=(const BufferBase&) = default;

  BufferBase(BufferBase&& other) { swap(other); }
  BufferBase& operator=(BufferBase&& other)
  {
    reset();
    swap(other);
    return *this;
  }

  void assign(PtrType ptr, size_type new_size) { BufferBase{std::move(ptr), new_size}.swap(*this); }
  void reset(size_type new_size = 0) { BufferBase{new_size}.swap(*this); }
  void clear() { reset(); }

  // Resize is purposely not provided as it often unnecessarily copies data about to be overwritten.
  void resize(std::size_t) = delete;

  std::pair<PtrType, size_type> extract()
  {
    std::pair result = {std::move(m_ptr), m_size};
    reset();
    return result;
  }

  void swap(BufferBase& other)
  {
    std::swap(m_ptr, other.m_ptr);
    std::swap(m_size, other.m_size);
  }

  size_type size() const { return m_size; }
  bool empty() const { return m_size == 0; }

  value_type* get() { return m_ptr.get(); }
  const value_type* get() const { return m_ptr.get(); }

  value_type* data() { return m_ptr.get(); }
  const value_type* data() const { return m_ptr.get(); }

  value_type* begin() { return m_ptr.get(); }
  value_type* end() { return m_ptr.get() + m_size; }

  const value_type* begin() const { return m_ptr.get(); }
  const value_type* end() const { return m_ptr.get() + m_size; }

  value_type& operator[](size_type index) { return m_ptr[index]; }
  const value_type& operator[](size_type index) const { return m_ptr[index]; }

private:
  PtrType m_ptr;
  size_type m_size = 0;
};
}  // namespace detail

template <typename T>
using UniqueBuffer = detail::BufferBase<std::make_unique_for_overwrite<T[]>>;

// TODO: std::make_shared_for_overwrite requires GCC 12.1+
// template <typename T>
// using SharedBuffer = detail::BufferBase<std::make_shared_for_overwrite<T[]>>;

}  // namespace Common
