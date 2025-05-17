// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <ranges>

// An array-backed (or other storage) circular queue.
//
// Push/Pop increases/decrease the Size()
//
// Capacity() gives size of the underlying buffer.
//
// Push'ing Size() beyond Capacity() is unchecked and legal.
//  It will overwrite not-yet-read data.
//
// Pop'ing Size() below zero is unchecked and *not* allowed.
//
// Write/Read heads can be manually adjusted for your various weird needs.
// AdvanceWriteHead / AdvanceReadHead / RewindReadHead
//
// Thread-safe version: SPSCCircularQueue

namespace Common
{

namespace detail
{

template <typename ElementType, typename StorageType, bool UseAtomicSize,
          bool IncludeWaitFunctionality>
class CircularQueueBase
{
public:
  static_assert(std::is_trivially_copyable_v<ElementType>,
                "Really only sane for trivially copyable types.");

  // Arguments are forwarded to the storage constructor.
  template <typename... Args>
  explicit CircularQueueBase(Args&&... args) : m_storage(std::forward<Args>(args)...)
  {
  }

  // "Push" functions are only safe from the "producer" thread.
  ElementType& GetWriteHead() { return m_storage[m_write_index]; }

  void Push(const ElementType& value)
  {
    GetWriteHead() = value;
    AdvanceWriteHead(1);
  }

  // Returns a span from the write-head to the end of the buffer.
  // Elements may be written here before using AdvanceWriteHead.
  std::span<ElementType> GetWriteSpan()
  {
    return std::span(&GetWriteHead(), m_storage.size() - m_write_index);
  }

  // Push elements from range.
  // Will overwrite existing data if Capacity is exceeded.
  template <std::ranges::range R>
  requires(std::is_convertible_v<typename R::value_type, ElementType>)
  void PushRange(const R& range)
  {
    auto input_iter = range.begin();
    const std::size_t range_size = std::size(range);
    std::size_t remaining = range_size;
    auto output_iter = m_storage.begin() + m_write_index;

    while (remaining != 0)
    {
      const std::size_t count = std::min<std::size_t>(m_storage.end() - output_iter, remaining);
      input_iter = std::ranges::copy_n(input_iter, count, output_iter).in;

      remaining -= count;
      output_iter = m_storage.begin();
    }

    AdvanceWriteHead(range_size);
  }

  // May be used by the "producer" thread.
  // Moves the write position and increases the size without writing any data.
  void AdvanceWriteHead(std::size_t count, std::memory_order order = std::memory_order_release)
  {
    m_write_index = (m_write_index + count) % Capacity();
    AdjustSize(count, order);
  }

  // "Pop" functions are only safe from the "consumer" thread.
  const ElementType& GetReadHead() const { return m_storage[m_read_index]; }

  ElementType Pop()
  {
    assert(Size() != 0);

    ElementType result = GetReadHead();
    AdvanceReadHead(1);
    return result;
  }

  // Returns a span from the read-head to the end of the buffer.
  // Not Size() checked. Caller must verify elements have been written.
  std::span<const ElementType> GetReadSpan() const
  {
    return std::span(&GetReadHead(), m_storage.size() - m_read_index);
  }

  // Pop elements to fill the provided range.
  // Does not check first if this many elements are readable.
  template <std::ranges::range R>
  requires(std::is_convertible_v<ElementType, typename R::value_type>)
  void PopRange(R* range)
  {
    auto output_iter = range->begin();
    const std::size_t range_size = std::size(*range);
    std::size_t remaining = range_size;
    auto input_iter = m_storage.begin() + m_read_index;

    while (remaining != 0)
    {
      const std::size_t count = std::min<std::size_t>(m_storage.end() - input_iter, remaining);
      output_iter = std::ranges::copy_n(input_iter, count, output_iter).out;

      remaining -= count;
      input_iter = m_storage.begin();
    }

    AdvanceReadHead(range_size);
  }

  // Used by the "consumer" thread to move the read position without reading any data.
  // Also reduces size by the given amount.
  // Use RewindReadHead for negative values.
  void AdvanceReadHead(std::size_t count)
  {
    m_read_index = (m_read_index + count) % Capacity();
    AdjustSize(0 - count, std::memory_order_relaxed);
  }

  // Used by the "consumer" to move the read position backwards.
  // Also increases the size by the given amount.
  void RewindReadHead(std::size_t count)
  {
    const auto capacity = Capacity();
    m_read_index = (m_read_index + capacity - count % capacity) % capacity;
    AdjustSize(count, std::memory_order_relaxed);
  }

  // Only safe from the "consumer".
  void Clear() { AdvanceReadHead(Size(std::memory_order_relaxed)); }

  // The following are safe from any thread.
  [[nodiscard]] bool Empty(std::memory_order order = std::memory_order_acquire) const
  {
    return Size(order) == 0;
  }

  // The size of the underlying storage (the size of the ring buffer).
  [[nodiscard]] std::size_t Capacity() const { return m_storage.size(); }

  // The number of Push'd elements yet to be Pop'd.
  // Exceeding the Capacity which will cause data to be overwritten and read multiple times.
  [[nodiscard]] std::size_t Size(std::memory_order order = std::memory_order_acquire) const
  {
    if constexpr (UseAtomicSize)
      return m_size.load(order);
    else
      return m_size;
  }

  // Number of elements that can be written before reaching Capacity.
  [[nodiscard]] std::size_t Writable() const
  {
    const auto capacity = Capacity();
    return capacity - std::min(capacity, Size(std::memory_order_relaxed));
  }

  [[nodiscard]] std::size_t Readable() const { return Size(); }

  // The following are "safe" from any thread,
  //  but e.g. using WaitForData from the producer would just block indefinitely.

  // Blocks until size is something other than the provided value.
  void WaitForSizeChange(std::size_t old_size,
                         std::memory_order order = std::memory_order_acquire) const
      requires(IncludeWaitFunctionality)
  {
    m_size.wait(old_size, order);
  }

  // Blocks until (Size() + count) <= Capacity().
  void WaitForWritable(std::size_t count = 1) const requires(IncludeWaitFunctionality)
  {
    assert(count <= Capacity());

    std::size_t old_size = 0;
    while ((old_size = Size(std::memory_order_relaxed)) > (Capacity() - count))
      WaitForSizeChange(old_size, std::memory_order_relaxed);
  }

  // Blocks until Size() == 0.
  void WaitForEmpty() const requires(IncludeWaitFunctionality) { WaitForWritable(Capacity()); }

  // Blocks until Size() >= count.
  void WaitForReadable(std::size_t count = 1) const requires(IncludeWaitFunctionality)
  {
    std::size_t old_size = 0;
    while ((old_size = Size()) < count)
      WaitForSizeChange(old_size);
  }

private:
  void AdjustSize(std::size_t count, std::memory_order order)
  {
    if constexpr (UseAtomicSize)
      m_size.fetch_add(count, order);
    else
      m_size += count;

    if constexpr (IncludeWaitFunctionality)
      m_size.notify_one();
  }

  StorageType m_storage;

  std::conditional_t<UseAtomicSize, std::atomic<std::size_t>, std::size_t> m_size = 0;

  std::size_t m_read_index = 0;
  std::size_t m_write_index = 0;
};

}  // namespace detail

template <typename T, typename StorageType>
using CircularQueue = detail::CircularQueueBase<T, StorageType, false, false>;

template <typename T, std::size_t Capacity>
using CircularArray = CircularQueue<T, std::array<T, Capacity>>;

template <typename T, typename StorageType>
using SPSCCircularQueue = detail::CircularQueueBase<T, StorageType, true, false>;

template <typename T, std::size_t Capacity>
using SPSCCircularArray = SPSCCircularQueue<T, std::array<T, Capacity>>;

template <typename T, typename StorageType>
using WaitableSPSCCircularQueue = detail::CircularQueueBase<T, StorageType, true, true>;

template <typename T, std::size_t Capacity>
using WaitableSPSCCircularArray = WaitableSPSCCircularQueue<T, std::array<T, Capacity>>;

}  // namespace Common
