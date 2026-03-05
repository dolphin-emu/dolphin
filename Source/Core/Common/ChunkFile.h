// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// Extremely simple serialization framework.

// (mis)-features:
// + Super fast
// + Very simple
// + Same code is used for serialization and deserialization (in most cases)
// - Zero backwards/forwards compatibility
// - Serialization code for anything complex has to be manually written.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <deque>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Flag.h"
#include "Common/Inline.h"

namespace detail
{

template <typename T>
static constexpr bool IsSafeToMemcpy = std::is_trivially_copyable_v<T> && !std::is_const_v<T> &&
                                       !std::is_pointer_v<T> && !std::is_same_v<T, bool>;

// FYI: GCC <= 13 seems to require these specializations be in namespace rather than class scope.

// std::optional<T> may be trivially copyable but the layout isn't necessarily portable.
template <typename T>
static constexpr bool IsSafeToMemcpy<std::optional<T>> = false;

template <typename T1, typename T2>
static constexpr bool IsSafeToMemcpy<std::pair<T1, T2>> = IsSafeToMemcpy<T1> && IsSafeToMemcpy<T2>;

}  // namespace detail

// Wrapper class
class PointerWrap
{
public:
  enum class Mode
  {
    Read,
    Write,
    Measure,
    Verify,
  };

private:
  u8* m_ptr_current;
  u8* const m_ptr_begin;
  const u8* const m_ptr_end;
  Mode m_mode;

public:
  PointerWrap(std::span<u8> data, Mode mode)
      : m_ptr_current{data.data()}, m_ptr_begin{m_ptr_current},
        m_ptr_end{m_ptr_current + data.size()}, m_mode{mode}
  {
  }

  void SetMeasureMode() { m_mode = Mode::Measure; }
  void SetVerifyMode() { m_mode = Mode::Verify; }

  bool IsReadMode() const { return m_mode == Mode::Read; }
  bool IsWriteMode() const { return m_mode == Mode::Write; }
  bool IsMeasureMode() const { return m_mode == Mode::Measure; }
  bool IsVerifyMode() const { return m_mode == Mode::Verify; }

  template <typename K, class V>
  void Do(std::map<K, V>& x)
  {
    auto count = u32(x.size());
    Do(count);

    if (m_mode == Mode::Read)
    {
      std::pair<K, V> value{};
      for (x.clear(); count != 0; --count)
      {
        Do(value);
        x.insert(x.end(), std::move(value));
      }
    }
    else
    {
      for (auto& value : x)
      {
        Do(const_cast<K&>(value.first));
        Do(value.second);
      }
    }
  }

  template <typename V>
  void Do(std::set<V>& x)
  {
    auto count = u32(x.size());
    Do(count);

    if (m_mode == Mode::Read)
    {
      V value{};
      for (x.clear(); count != 0; --count)
      {
        Do(value);
        x.insert(x.end(), std::move(value));
      }
    }
    else
    {
      for (auto& value : x)
        Do(const_cast<V&>(value));
    }
  }

  template <typename T>
  void Do(std::vector<T>& x)
  {
    DoContiguousContainer(x);
  }

  template <typename T>
  void Do(std::list<T>& x)
  {
    DoContainer(x);
  }

  template <typename T>
  void Do(std::deque<T>& x)
  {
    DoContainer(x);
  }

  template <typename T>
  void Do(std::basic_string<T>& x)
  {
    DoContiguousContainer(x);
  }

  template <typename T, typename U>
  void Do(std::pair<T, U>& x)
  {
    Do(x.first);
    Do(x.second);
  }

  template <typename T>
  void Do(std::optional<T>& x)
  {
    bool present = x.has_value();
    Do(present);

    switch (m_mode)
    {
    case Mode::Read:
      if (present)
      {
        Do(x.emplace());
      }
      else
      {
        x = std::nullopt;
      }
      break;

    case Mode::Write:
    case Mode::Measure:
    case Mode::Verify:
      if (present)
        Do(x.value());

      break;
    }
  }

  template <typename T, std::size_t N>
  void DoArray(std::array<T, N>& x)
  {
    DoArray(x.data(), static_cast<u32>(x.size()));
  }

  template <typename T>
  requires(detail::IsSafeToMemcpy<T>)
  void DoArray(T* x, u32 count)
  {
    DoVoid(x, count * sizeof(T));
  }

  template <typename T>
  requires(!detail::IsSafeToMemcpy<T>)
  void DoArray(T* x, u32 count)
  {
    for (u32 i = 0; i < count; ++i)
      Do(x[i]);
  }

  template <typename T, std::size_t N>
  void DoArray(T (&arr)[N])
  {
    DoArray(arr, static_cast<u32>(N));
  }

  // The caller is required to inspect the mode of this PointerWrap
  // and deal with the pointer returned from this function themself.
  [[nodiscard]] u8* DoExternal(u32& count)
  {
    Do(count);
    u8* current = m_ptr_current;
    m_ptr_current += count;
    if (!IsMeasureMode() && m_ptr_current > m_ptr_end)
    {
      // trying to read/write past the end of the buffer, prevent this
      SetMeasureMode();
    }
    return current;
  }

  // The reserved u32 is set to 0, and a pointer to it is returned.
  // The caller needs to fill in the reserved u32 with the appropriate value later on, if they
  // want a non-zero value there.
  [[nodiscard]] u8* ReserveU32()
  {
    u32 temp = 0;
    u8* const previous_pointer = m_ptr_current;
    Do(temp);
    return previous_pointer;
  }

  u32 GetOffsetFromPreviousPosition(u8* previous_pointer) const
  {
    return static_cast<u32>((m_ptr_current)-previous_pointer);
  }

  u32 GetCurrentOffset() const { return GetOffsetFromPreviousPosition(m_ptr_begin); }

  void Do(Common::Flag& flag)
  {
    bool s = flag.IsSet();
    Do(s);
    if (IsReadMode())
      flag.Set(s);
  }

  template <typename T>
  void Do(std::atomic<T>& atomic)
  {
    T temp = atomic.load(std::memory_order_relaxed);
    Do(temp);
    if (IsReadMode())
      atomic.store(temp, std::memory_order_relaxed);
  }

  template <typename T>
  requires(detail::IsSafeToMemcpy<T>)
  void Do(T& x)
  {
    DoVoid(&x, sizeof(x));
  }

  void Do(bool& x)
  {
    // bool's size is platform dependant and not portable.
    // This treats all bools as if they were 8 bits in size.
    u8 value = static_cast<u8>(x);

    Do(value);

    if (IsReadMode())
      x = value != 0;
  }

  template <typename T>
  void DoPointer(T*& x, T* const base)
  {
    // pointers can be more than 2^31 apart, but you're using this function wrong if you need that
    // much range
    ptrdiff_t offset = x - base;
    Do(offset);
    if (IsReadMode())
    {
      x = base + offset;
    }
  }

  void DoMarker(const std::string& previous_name, u32 arbitrary_number = 0x42)
  {
    u32 cookie = arbitrary_number;
    Do(cookie);

    if (IsReadMode() && cookie != arbitrary_number)
    {
      PanicAlertFmtT(
          "Error: After \"{0}\", found {1} ({2:#x}) instead of save marker {3} ({4:#x}). Aborting "
          "savestate load...",
          previous_name, cookie, cookie, arbitrary_number, arbitrary_number);
      SetMeasureMode();
    }
  }

  template <typename T, typename Functor>
  void DoEachElement(T& container, Functor member)
  {
    u32 size = static_cast<u32>(container.size());
    Do(size);
    container.resize(size);

    for (auto& elem : container)
      member(*this, elem);
  }

private:
  template <typename T>
  void DoContiguousContainer(T& container)
  {
    u32 size = static_cast<u32>(container.size());
    Do(size);

    container.resize(size);
    DoArray(container.data(), size);
  }

  template <typename T>
  void DoContainer(T& x)
  {
    DoEachElement(x, [](PointerWrap& p, typename T::value_type& elem) { p.Do(elem); });
  }

  DOLPHIN_FORCE_INLINE void DoVoid(void* data, u32 size)
  {
    if (!IsMeasureMode() && (m_ptr_current + size) > m_ptr_end)
    {
      // trying to read/write past the end of the buffer, prevent this
      SetMeasureMode();
    }

    switch (m_mode)
    {
    case Mode::Read:
      std::memcpy(data, m_ptr_current, size);
      break;

    case Mode::Write:
      std::memcpy(m_ptr_current, data, size);
      break;

    case Mode::Measure:
      break;

    case Mode::Verify:
      DEBUG_ASSERT_MSG(COMMON, !std::memcmp(data, m_ptr_current, size),
                       "Savestate verification failure: buf {} != {} (size {}).\n", fmt::ptr(data),
                       fmt::ptr(m_ptr_current), size);
      break;
    }

    m_ptr_current += size;
  }
};
