#pragma once

#include <cstddef>
#include <fmt/format.h>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdio.h>
#include <type_traits>

#include "Common/CommonTypes.h"
#include "Common/Inline.h"

template <typename StorageType>
struct BitField2
{
public:
  constexpr StorageType Get() const { return storage; }
  constexpr void Set(StorageType val) { storage = val; }
  constexpr StorageType& GetStorageRef() { return storage; }

  BitField2() = default;
  // Bare statement alternative for Set()
  constexpr BitField2(StorageType val) : storage(val){};

  // Bare statement alternative for Get()
  constexpr operator StorageType() const { return Get(); }

  constexpr BitField2& operator=(StorageType rhs)
  {
    Set(rhs);
    return *this;
  }
  constexpr BitField2& operator+=(StorageType rhs) { return operator=(Get() + rhs); }
  constexpr BitField2& operator-=(StorageType rhs) { return operator=(Get() - rhs); }
  constexpr BitField2& operator*=(StorageType rhs) { return operator=(Get() * rhs); }
  constexpr BitField2& operator/=(StorageType rhs) { return operator=(Get() / rhs); }
  constexpr BitField2& operator|=(StorageType rhs) { return operator=(Get() | rhs); }
  constexpr BitField2& operator&=(StorageType rhs) { return operator=(Get() & rhs); }
  constexpr BitField2& operator^=(StorageType rhs) { return operator=(Get() ^ rhs); }

public:
  StorageType storage;
  using StorageTypeU = std::make_unsigned_t<StorageType>;
  using StorageTypeS = std::make_signed_t<StorageType>;

public:
  /////////////////////////////////////////////////////////////////////////////////////////////////

  template <typename FieldType, std::size_t start, std::size_t width>
  struct FixedView
  {
  public:
    FixedView() = delete;
    FixedView(BitField2<StorageType>* host_) : host(host_){};

    // Bare statement alternative for Get()
    constexpr operator FieldType() const { return Get(); }

    constexpr static std::size_t rshift = 8 * sizeof(StorageType) - width;
    constexpr static std::size_t lshift = rshift - start;
    constexpr static StorageTypeU mask =
        std::numeric_limits<StorageTypeU>::max() >> rshift << start;  //

    DOLPHIN_FORCE_INLINE FieldType Get() const
    {
      // Necessary to choose between arithmatic and logical right-shift.
      // This should be all constexpr, so it should get optimized.
      if (std::is_signed<FieldType>())
        return static_cast<FieldType>(static_cast<StorageTypeS>(host->Get() << lshift) >> rshift);
      else
        return static_cast<FieldType>(static_cast<StorageTypeU>(host->Get() << lshift) >> rshift);
    }

    DOLPHIN_FORCE_INLINE void Set(FieldType val)
    {
      host->Set((host->Get() & ~mask) | ((static_cast<StorageType>(val) << start) & mask));
    }

    DOLPHIN_FORCE_INLINE FixedView& operator=(FieldType rhs)
    {
      Set(rhs);
      return *this;
    }
    DOLPHIN_FORCE_INLINE FixedView& operator+=(FieldType rhs) { return operator=(Get() + rhs); }
    DOLPHIN_FORCE_INLINE FixedView& operator-=(FieldType rhs) { return operator=(Get() - rhs); }
    DOLPHIN_FORCE_INLINE FixedView& operator*=(FieldType rhs) { return operator=(Get() * rhs); }
    DOLPHIN_FORCE_INLINE FixedView& operator/=(FieldType rhs) { return operator=(Get() / rhs); }
    DOLPHIN_FORCE_INLINE FixedView& operator|=(FieldType rhs) { return operator=(Get() / rhs); }
    DOLPHIN_FORCE_INLINE FixedView& operator&=(FieldType rhs) { return operator=(Get() & rhs); }
    DOLPHIN_FORCE_INLINE FixedView& operator^=(FieldType rhs) { return operator=(Get() ^ rhs); }

    DOLPHIN_FORCE_INLINE FixedView& operator=(FixedView& rhs) { return operator=(rhs.Get()); }

    DOLPHIN_FORCE_INLINE bool operator!() { return !Get(); }
    // Do not overload && or || because it incurs a penalty from losing short-circuit evaluation

  protected:
    BitField2<StorageType>* const host;
  };

  /////////////////////////////////////////////////////////////////////////////////////////////////

  template <typename FieldType, std::size_t start, std::size_t width>
  struct ConstFixedView
  {
  public:
    ConstFixedView() = delete;
    ConstFixedView(const BitField2<StorageType>* host_) : host(host_){};

    // Bare statement alternative for Get()
    constexpr operator FieldType() const { return Get(); }

    constexpr static std::size_t rshift = 8 * sizeof(StorageType) - width;
    constexpr static std::size_t lshift = rshift - start;

    DOLPHIN_FORCE_INLINE const FieldType Get() const
    {
      // Necessary to choose between arithmatic and logical right-shift.
      // This should be all constexpr, so it should get optimized.
      if (std::is_signed<FieldType>())
        return static_cast<FieldType>(static_cast<StorageTypeS>(host->Get() << lshift) >> rshift);
      else
        return static_cast<FieldType>(static_cast<StorageTypeU>(host->Get() << lshift) >> rshift);
    }

    DOLPHIN_FORCE_INLINE bool operator!() { return !Get(); }
    // Do not overload && or || because it incurs a penalty from losing short-circuit evaluation

  protected:
    const BitField2<StorageType>* const host;
  };

  /////////////////////////////////////////////////////////////////////////////////////////////////

  template <typename FieldType>
  struct View
  {
  public:
    View() = delete;
    View(BitField2<StorageType>* host_, std::size_t start_, std::size_t width_)
        : host(host_), start(start_), width(width_){};

    // Bare statement alternative for Get()
    constexpr operator FieldType() const { return Get(); }

    constexpr static StorageTypeU unsigned_limit = std::numeric_limits<StorageTypeU>::max();

    DOLPHIN_FORCE_INLINE FieldType Get() const
    {
      const std::size_t rshift = 8 * sizeof(StorageType) - width;
      const std::size_t lshift = rshift - start;
      // Necessary to choose between arithmatic and logical right-shift.
      // This should be all constexpr, so it should get optimized.
      if (std::is_signed<FieldType>())
        return static_cast<FieldType>(static_cast<StorageTypeS>(host->Get() << lshift) >> rshift);
      else
        return static_cast<FieldType>(static_cast<StorageTypeU>(host->Get() << lshift) >> rshift);
    }

    DOLPHIN_FORCE_INLINE void Set(FieldType val)
    {
      const std::size_t rshift = 8 * sizeof(StorageType) - width;
      const StorageTypeU mask = unsigned_limit >> rshift << start;
      host->Set((host->Get() & ~mask) | ((static_cast<StorageType>(val) << start) & mask));
    }

    DOLPHIN_FORCE_INLINE View& operator=(FieldType rhs)
    {
      Set(rhs);
      return *this;
    }
    DOLPHIN_FORCE_INLINE View& operator+=(FieldType rhs) { return operator=(Get() + rhs); }
    DOLPHIN_FORCE_INLINE View& operator-=(FieldType rhs) { return operator=(Get() - rhs); }
    DOLPHIN_FORCE_INLINE View& operator*=(FieldType rhs) { return operator=(Get() * rhs); }
    DOLPHIN_FORCE_INLINE View& operator/=(FieldType rhs) { return operator=(Get() / rhs); }
    DOLPHIN_FORCE_INLINE View& operator|=(FieldType rhs) { return operator=(Get() | rhs); }
    DOLPHIN_FORCE_INLINE View& operator&=(FieldType rhs) { return operator=(Get() & rhs); }
    DOLPHIN_FORCE_INLINE View& operator^=(FieldType rhs) { return operator=(Get() ^ rhs); }

    DOLPHIN_FORCE_INLINE View& operator=(View& rhs) { return operator=(rhs.Get()); }

    DOLPHIN_FORCE_INLINE bool operator!() { return !Get(); }
    // Do not overload && or || because it incurs a penalty from losing short-circuit evaluation

  protected:
    BitField2<StorageType>* const host;
    std::size_t start;
    std::size_t width;
  };

  /////////////////////////////////////////////////////////////////////////////////////////////////

  template <typename FieldType>
  struct ConstView
  {
  public:
    ConstView() = delete;
    ConstView(const BitField2<StorageType>* host_, std::size_t start_, std::size_t width_)
        : host(host_), start(start_), width(width_){};

    // Bare statement alternative for Get()
    constexpr operator FieldType() const { return Get(); }

    DOLPHIN_FORCE_INLINE const FieldType Get() const
    {
      const std::size_t rshift = 8 * sizeof(StorageType) - width;
      const std::size_t lshift = rshift - start;
      // Necessary to choose between arithmatic and logical right-shift.
      // This should be all constexpr, so it should get optimized.
      if (std::is_signed<FieldType>())
        return static_cast<FieldType>(static_cast<StorageTypeS>(host->Get() << lshift) >> rshift);
      else
        return static_cast<FieldType>(static_cast<StorageTypeU>(host->Get() << lshift) >> rshift);
    }

    DOLPHIN_FORCE_INLINE bool operator!() { return !Get(); }

  protected:
    const BitField2<StorageType>* const host;
    std::size_t start;
    std::size_t width;
  };

  /////////////////////////////////////////////////////////////////////////////////////////////////

  template <typename FieldType, std::size_t start, std::size_t width>
  struct FixedViewArrayIterator
  {
  public:
    // Required by std::input_or_output_iterator
    constexpr FixedViewArrayIterator() = default;
    // Required by LegacyIterator
    constexpr FixedViewArrayIterator(const FixedViewArrayIterator& other) = default;
    // Required by LegacyIterator
    FixedViewArrayIterator& operator=(const FixedViewArrayIterator& other) = default;
    // Move constructor and assignment operators, explicitly defined for completeness
    constexpr FixedViewArrayIterator(FixedViewArrayIterator&& other) = default;
    FixedViewArrayIterator& operator=(FixedViewArrayIterator&& other) = default;

    using iterator_category = std::output_iterator_tag;
    using value_type = FieldType;
    using difference_type = ptrdiff_t;
    using pointer = void;
    using reference = View<FieldType>;

    FixedViewArrayIterator(BitField2<StorageType>* host_, std::size_t idx_) : host(host_), idx(idx_)
    {
    }

  public:
    FixedViewArrayIterator& operator++()
    {
      ++idx;
      return *this;
    }
    FixedViewArrayIterator operator++(int)
    {
      FixedViewArrayIterator other(*this);
      ++*this;
      return other;
    }
    constexpr reference operator*() const
    {
      const std::size_t view_start = start + width * idx;
      return reference(host, view_start, width);
    }
    constexpr bool operator==(const FixedViewArrayIterator& other) const
    {
      return idx == other.idx;
    }
    constexpr bool operator!=(const FixedViewArrayIterator& other) const
    {
      return idx != other.idx;
    }

  protected:
    BitField2<StorageType>* const host;
    std::size_t idx;
  };

  /////////////////////////////////////////////////////////////////////////////////////////////////

  template <typename FieldType, std::size_t start, std::size_t width>
  struct FixedViewArrayConstIterator
  {
  public:
    // Required by std::input_or_output_iterator
    constexpr FixedViewArrayConstIterator() = default;
    // Required by LegacyIterator
    constexpr FixedViewArrayConstIterator(const FixedViewArrayConstIterator& other) = default;
    // Required by LegacyIterator
    FixedViewArrayConstIterator& operator=(const FixedViewArrayConstIterator& other) = default;
    // Move constructor and assignment operators, explicitly defined for completeness
    constexpr FixedViewArrayConstIterator(FixedViewArrayConstIterator&& other) = default;
    FixedViewArrayConstIterator& operator=(FixedViewArrayConstIterator&& other) = default;

    using iterator_category = std::input_iterator_tag;
    using value_type = FieldType;
    using difference_type = ptrdiff_t;
    using pointer = void;
    using reference = ConstView<FieldType>;

    FixedViewArrayConstIterator(const BitField2<StorageType>* host_, std::size_t idx_)
        : host(host_), idx(idx_)
    {
    }

  public:
    FixedViewArrayConstIterator& operator++()
    {
      ++idx;
      return *this;
    }
    FixedViewArrayConstIterator operator++(int)
    {
      FixedViewArrayConstIterator other(*this);
      ++*this;
      return other;
    }
    constexpr reference operator*() const
    {
      const std::size_t view_start = start + width * idx;
      return reference(host, view_start, width);
    }
    constexpr bool operator==(const FixedViewArrayConstIterator other) const
    {
      return idx == other.idx;
    }
    constexpr bool operator!=(const FixedViewArrayConstIterator other) const
    {
      return idx != other.idx;
    }

  protected:
    const BitField2<StorageType>* const host;
    std::size_t idx;
  };

  /////////////////////////////////////////////////////////////////////////////////////////////////

  template <typename FieldType, std::size_t start, std::size_t width, std::size_t length>
  struct FixedViewArray
  {
  public:
    FixedViewArray() = delete;
    FixedViewArray(BitField2<StorageType>* host_) : host(host_) {}

    constexpr static std::size_t shift_amount = 8 * sizeof(StorageType) - width;
    constexpr static auto unsigned_limit = std::numeric_limits<StorageTypeU>::max();

    DOLPHIN_FORCE_INLINE FieldType Get(std::size_t idx) const
    {
      static_assert(idx < length, "Index out of range");
      const std::size_t pos = start + width * idx;
      return static_cast<FieldType>((host->Get() << (shift_amount - pos)) >> shift_amount);
    }

    DOLPHIN_FORCE_INLINE void Set(std::size_t idx, FieldType val)
    {
      static_assert(idx < length, "Index out of range");
      const std::size_t pos = start + width * idx;
      const StorageTypeU mask = (unsigned_limit >> shift_amount) << pos;
      host->Set((host->Get() & ~mask) | ((static_cast<StorageType>(val) << pos) & mask));
    }

    View<FieldType> operator[](std::size_t idx)
    {
      // static_assert(idx < length, "Index out of range");
      const std::size_t view_start = start + width * idx;
      return View<FieldType>(host, view_start, width);
    }
    ConstView<FieldType> operator[](std::size_t idx) const
    {
      // static_assert(idx < length, "Index out of range");
      const std::size_t view_start = start + width * idx;
      return ConstView<FieldType>(host, view_start, width);
    }

    using Iterator = FixedViewArrayIterator<FieldType, start, width>;
    using ConstIterator = FixedViewArrayConstIterator<FieldType, start, width>;

    constexpr Iterator begin() { return Iterator(host, 0); }
    constexpr Iterator end() { return Iterator(host, length); }
    constexpr ConstIterator begin() const { return ConstIterator(host, 0); }
    constexpr ConstIterator end() const { return ConstIterator(host, length); }
    constexpr ConstIterator cbegin() const { return ConstIterator(host, 0); }
    constexpr ConstIterator cend() const { return ConstIterator(host, length); }
    constexpr std::size_t Size() const { return length; }
    constexpr std::size_t Length() const { return length; }

  protected:
    BitField2<StorageType>* const host;
  };

  /////////////////////////////////////////////////////////////////////////////////////////////////

  template <typename FieldType, std::size_t start, std::size_t width, std::size_t length>
  struct ConstFixedViewArray
  {
  public:
    ConstFixedViewArray() = delete;
    ConstFixedViewArray(const BitField2<StorageType>* host_) : host(host_) {}

    constexpr static std::size_t shift_amount = 8 * sizeof(StorageType) - width;
    constexpr static auto unsigned_limit = std::numeric_limits<StorageTypeU>::max();

    DOLPHIN_FORCE_INLINE FieldType Get(std::size_t idx) const
    {
      // static_assert(idx < length, "Index out of range");
      const std::size_t pos = start + width * idx;
      return static_cast<FieldType>((host->Get() << (shift_amount - pos)) >> shift_amount);
    }

    ConstView<FieldType> operator[](std::size_t idx) const
    {
      // static_assert(idx < length, "Index out of range");
      const std::size_t view_start = start + width * idx;
      return ConstView<FieldType>(host, view_start, width);
    }

    using ConstIterator = FixedViewArrayConstIterator<FieldType, start, width>;

    constexpr ConstIterator begin() const { return ConstIterator(host, 0); }
    constexpr ConstIterator end() const { return ConstIterator(host, length); }
    constexpr ConstIterator cbegin() const { return ConstIterator(host, 0); }
    constexpr ConstIterator cend() const { return ConstIterator(host, length); }
    constexpr std::size_t Size() const { return length; }
    constexpr std::size_t Length() const { return length; }

  protected:
    const BitField2<StorageType>* const host;
  };
};

// FMT library stuff vvvvvvv
/*
template <typename StorageType>
template <typename FieldType, std::size_t start, std::size_t width>
struct fmt::formatter<typename BitField2<StorageType>::template FixedView<FieldType, start, width>>
{
  fmt::formatter<FieldType> m_formatter;
  constexpr auto parse(format_parse_context& ctx) { return m_formatter.parse(ctx); }
  template <typename FormatContext>
  auto
  format(const typename BitField2<StorageType>::template FixedView<FieldType, start, width>& ref,
         FormatContext& ctx) const
  {
    return m_formatter.format(ref.Get(), ctx);
  }
};

template <typename StorageType>
template <typename FieldType, std::size_t start, std::size_t width>
struct fmt::formatter<
    typename BitField2<StorageType>::template ConstFixedView<FieldType, start, width>>
{
  fmt::formatter<FieldType> m_formatter;
  constexpr auto parse(format_parse_context& ctx) { return m_formatter.parse(ctx); }
  template <typename FormatContext>
  auto format(
      const typename BitField2<StorageType>::template ConstFixedView<FieldType, start, width>& ref,
      FormatContext& ctx) const
  {
    return m_formatter.format(ref.Get(), ctx);
  }
};

template <typename StorageType>
template <typename FieldType>
struct fmt::formatter<typename BitField2<StorageType>::template View<FieldType>>
{
  fmt::formatter<FieldType> m_formatter;
  constexpr auto parse(format_parse_context& ctx) { return m_formatter.parse(ctx); }
  template <typename FormatContext>
  auto format(const typename BitField2<StorageType>::template View<FieldType>& ref,
              FormatContext& ctx) const
  {
    return m_formatter.format(ref.Get(), ctx);
  }
};

template <typename StorageType>
template <typename FieldType>
struct fmt::formatter<typename BitField2<StorageType>::template ConstView<FieldType>>
{
  fmt::formatter<FieldType> m_formatter;
  constexpr auto parse(format_parse_context& ctx) { return m_formatter.parse(ctx); }
  template <typename FormatContext>
  auto format(const typename BitField2<StorageType>::template ConstView<FieldType>& ref,
              FormatContext& ctx) const
  {
    return m_formatter.format(ref.Get(), ctx);
  }
};
*/
// FMT library stuff ^^^^^^

// For inheritance-based polymorphism, assume the this pointer is the host BitField.
#define CONSTFIELD(FieldType, start, width, name)                                                  \
  DOLPHIN_FORCE_INLINE ConstFixedView<const FieldType, start, width> name() const                  \
  {                                                                                                \
    return ConstFixedView<const FieldType, start, width>(this);                                    \
  }
#define FIELD(FieldType, start, width, name)                                                       \
  DOLPHIN_FORCE_INLINE FixedView<FieldType, start, width> name()                                   \
  {                                                                                                \
    return FixedView<FieldType, start, width>(this);                                               \
  }                                                                                                \
  CONSTFIELD(FieldType, start, width, name)
#define CONSTFIELDARRAY(FieldType, start, width, length, name)                                     \
  DOLPHIN_FORCE_INLINE ConstFixedViewArray<const FieldType, start, width, length> name() const     \
  {                                                                                                \
    return ConstFixedViewArray<const FieldType, start, width, length>(this);                       \
  }
#define FIELDARRAY(FieldType, start, width, length, name)                                          \
  DOLPHIN_FORCE_INLINE FixedViewArray<FieldType, start, width, length> name()                      \
  {                                                                                                \
    return FixedViewArray<FieldType, start, width, length>(this);                                  \
  }                                                                                                \
  CONSTFIELDARRAY(FieldType, start, width, length, name)

// For composition-based polymorphism, give the ability to specify a host BitField.
#define CONSTFIELD_IN(host, FieldType, start, width, name)                                         \
  DOLPHIN_FORCE_INLINE decltype(host)::ConstFixedView<const FieldType, start, width> name() const  \
  {                                                                                                \
    return decltype(host)::ConstFixedView<const FieldType, start, width>(&host);                   \
  }
#define FIELD_IN(host, FieldType, start, width, name)                                              \
  DOLPHIN_FORCE_INLINE decltype(host)::FixedView<FieldType, start, width> name()                   \
  {                                                                                                \
    return decltype(host)::FixedView<FieldType, start, width>(&host);                              \
  }                                                                                                \
  CONSTFIELD_IN(host, FieldType, start, width, name)
#define CONSTFIELDARRAY_IN(host, FieldType, start, width, length, name)                            \
  DOLPHIN_FORCE_INLINE decltype(host)::ConstFixedViewArray<const FieldType, start, width, length>  \
  name() const                                                                                     \
  {                                                                                                \
    return decltype(host)::ConstFixedViewArray<const FieldType, start, width, length>(&host);      \
  }
#define FIELDARRAY_IN(host, FieldType, start, width, length, name)                                 \
  DOLPHIN_FORCE_INLINE decltype(host)::FixedViewArray<FieldType, start, width, length> name()      \
  {                                                                                                \
    return decltype(host)::FixedViewArray<FieldType, start, width, length>(&host);                 \
  }                                                                                                \
  CONSTFIELDARRAY_IN(host, FieldType, start, width, length, name)

// TODO ConstFixedViewArray
