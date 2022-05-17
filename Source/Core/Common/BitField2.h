#pragma once

#include <cassert>
#include <cstddef>
#include <iterator>
#include <limits>

template <typename StorageType>
struct BitField2
{
  constexpr BitField2() = default;
  constexpr BitField2(const StorageType val) : storage(val){};

  constexpr operator StorageType() const { return Get(); }

  constexpr StorageType Get() const { return storage; }
  constexpr void Set(const StorageType val) { storage = val; }

  constexpr BitField2& operator=(const StorageType rhs)
  {
    Set(rhs);
    return *this;
  }
  constexpr BitField2& operator+=(const StorageType rhs) { return operator=(Get() + rhs); }
  constexpr BitField2& operator-=(const StorageType rhs) { return operator=(Get() - rhs); }
  constexpr BitField2& operator*=(const StorageType rhs) { return operator=(Get() * rhs); }
  constexpr BitField2& operator/=(const StorageType rhs) { return operator=(Get() / rhs); }
  constexpr BitField2& operator|=(const StorageType rhs) { return operator=(Get() | rhs); }
  constexpr BitField2& operator&=(const StorageType rhs) { return operator=(Get() & rhs); }
  constexpr BitField2& operator^=(const StorageType rhs) { return operator=(Get() ^ rhs); }

  StorageType storage;
  using StorageTypeU = std::make_unsigned_t<StorageType>;
  using StorageTypeS = std::make_signed_t<StorageType>;

  /////////////////////////////////////////////////////////////////////////////////////////////////

  template <typename FieldType, std::size_t start, std::size_t width>
  struct FixedView
  {
    FixedView() = delete;
    constexpr FixedView(BitField2<StorageType>* host_) : host(host_){};

    constexpr operator FieldType() const { return Get(); }
    constexpr FixedView& operator=(const FixedView& rhs) { return operator=(rhs.Get()); }

    constexpr static std::size_t rshift = 8 * sizeof(StorageType) - width;
    constexpr static std::size_t lshift = rshift - start;

    constexpr FieldType Get() const
    {
      // Choose between arithmatic (sign extend) or logical (no sign extend) right-shift.
      if (std::is_signed<FieldType>())
        return static_cast<FieldType>(static_cast<StorageTypeS>(host->Get() << lshift) >> rshift);
      else
        return static_cast<FieldType>(static_cast<StorageTypeU>(host->Get() << lshift) >> rshift);
    }

    constexpr void Set(const FieldType val)
    {
      constexpr StorageTypeU mask = std::numeric_limits<StorageTypeU>::max() >> rshift << start;
      host->Set((host->Get() & ~mask) | ((static_cast<StorageType>(val) << start) & mask));
    }

    constexpr FixedView& operator=(const FieldType rhs)
    {
      Set(rhs);
      return *this;
    }
    constexpr FixedView& operator+=(const FieldType rhs) { return operator=(Get() + rhs); }
    constexpr FixedView& operator-=(const FieldType rhs) { return operator=(Get() - rhs); }
    constexpr FixedView& operator*=(const FieldType rhs) { return operator=(Get() * rhs); }
    constexpr FixedView& operator/=(const FieldType rhs) { return operator=(Get() / rhs); }
    constexpr FixedView& operator|=(const FieldType rhs) { return operator=(Get() / rhs); }
    constexpr FixedView& operator&=(const FieldType rhs) { return operator=(Get() & rhs); }
    constexpr FixedView& operator^=(const FieldType rhs) { return operator=(Get() ^ rhs); }

  protected:
    BitField2<StorageType>* const host;
  };

  /////////////////////////////////////////////////////////////////////////////////////////////////

  template <typename FieldType, std::size_t start, std::size_t width>
  struct ConstFixedView
  {
    ConstFixedView() = delete;
    constexpr ConstFixedView(const BitField2<StorageType>* host_) : host(host_){};

    constexpr operator FieldType() const { return Get(); }

    constexpr static std::size_t rshift = 8 * sizeof(StorageType) - width;
    constexpr static std::size_t lshift = rshift - start;

    constexpr const FieldType Get() const
    {
      // Choose between arithmatic (sign extend) or logical (no sign extend) right-shift.
      if (std::is_signed<FieldType>())
        return static_cast<FieldType>(static_cast<StorageTypeS>(host->Get() << lshift) >> rshift);
      else
        return static_cast<FieldType>(static_cast<StorageTypeU>(host->Get() << lshift) >> rshift);
    }

  private:
    const BitField2<StorageType>* const host;
  };

  /////////////////////////////////////////////////////////////////////////////////////////////////

  template <typename FieldType>
  struct View
  {
    View() = delete;
    constexpr View(BitField2<StorageType>* host_, std::size_t start_, std::size_t width_)
        : host(host_), start(start_), width(width_){};

    constexpr operator FieldType() const { return Get(); }
    constexpr View& operator=(const View& rhs) { return operator=(rhs.Get()); }

    constexpr FieldType Get() const
    {
      const std::size_t rshift = 8 * sizeof(StorageType) - width;
      const std::size_t lshift = rshift - start;
      // Choose between arithmatic (sign extend) or logical (no sign extend) right-shift.
      if (std::is_signed<FieldType>())
        return static_cast<FieldType>(static_cast<StorageTypeS>(host->Get() << lshift) >> rshift);
      else
        return static_cast<FieldType>(static_cast<StorageTypeU>(host->Get() << lshift) >> rshift);
    }

    constexpr void Set(const FieldType val)
    {
      const std::size_t rshift = 8 * sizeof(StorageType) - width;
      const StorageTypeU mask = std::numeric_limits<StorageTypeU>::max() >> rshift << start;
      host->Set((host->Get() & ~mask) | ((static_cast<StorageType>(val) << start) & mask));
    }

    constexpr View& operator=(const FieldType rhs)
    {
      Set(rhs);
      return *this;
    }
    constexpr View& operator+=(const FieldType rhs) { return operator=(Get() + rhs); }
    constexpr View& operator-=(const FieldType rhs) { return operator=(Get() - rhs); }
    constexpr View& operator*=(const FieldType rhs) { return operator=(Get() * rhs); }
    constexpr View& operator/=(const FieldType rhs) { return operator=(Get() / rhs); }
    constexpr View& operator|=(const FieldType rhs) { return operator=(Get() | rhs); }
    constexpr View& operator&=(const FieldType rhs) { return operator=(Get() & rhs); }
    constexpr View& operator^=(const FieldType rhs) { return operator=(Get() ^ rhs); }

  private:
    BitField2<StorageType>* const host;
    std::size_t start;
    std::size_t width;
  };

  /////////////////////////////////////////////////////////////////////////////////////////////////

  template <typename FieldType>
  struct ConstView
  {
    ConstView() = delete;
    constexpr ConstView(const BitField2<StorageType>* host_, std::size_t start_, std::size_t width_)
        : host(host_), start(start_), width(width_){};

    constexpr operator FieldType() const { return Get(); }

    constexpr const FieldType Get() const
    {
      const std::size_t rshift = 8 * sizeof(StorageType) - width;
      const std::size_t lshift = rshift - start;
      // Choose between arithmatic (sign extend) or logical (no sign extend) right-shift.
      if (std::is_signed<FieldType>())
        return static_cast<FieldType>(static_cast<StorageTypeS>(host->Get() << lshift) >> rshift);
      else
        return static_cast<FieldType>(static_cast<StorageTypeU>(host->Get() << lshift) >> rshift);
    }

  private:
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
      return View<FieldType>(host, start + width * idx, width);
    }
    constexpr bool operator==(const FixedViewArrayIterator& other) const
    {
      return idx == other.idx;
    }
    constexpr bool operator!=(const FixedViewArrayIterator& other) const
    {
      return idx != other.idx;
    }

  private:
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
      return ConstView<FieldType>(host, start + width * idx, width);
    }
    constexpr bool operator==(const FixedViewArrayConstIterator other) const
    {
      return idx == other.idx;
    }
    constexpr bool operator!=(const FixedViewArrayConstIterator other) const
    {
      return idx != other.idx;
    }

  private:
    const BitField2<StorageType>* const host;
    std::size_t idx;
  };

  /////////////////////////////////////////////////////////////////////////////////////////////////

  template <typename FieldType, std::size_t start, std::size_t width, std::size_t length>
  struct FixedViewArray
  {
    FixedViewArray() = delete;
    constexpr FixedViewArray(BitField2<StorageType>* host_) : host(host_) {}

    constexpr static std::size_t rshift = 8 * sizeof(StorageType) - width;

    constexpr FieldType Get(const std::size_t idx) const
    {
      assert(idx < length);  // Index out of range
      const std::size_t lshift = rshift - (start + width * idx);
      // Choose between arithmatic (sign extend) or logical (no sign extend) right-shift.
      if (std::is_signed<FieldType>())
        return static_cast<FieldType>(static_cast<StorageTypeS>(host->Get() << lshift) >> rshift);
      else
        return static_cast<FieldType>(static_cast<StorageTypeU>(host->Get() << lshift) >> rshift);
    }
    constexpr void Set(const std::size_t idx, const FieldType val)
    {
      assert(idx < length);  // Index out of range
      const StorageTypeU mask = (std::numeric_limits<StorageTypeU>::max() >> rshift)
                                << (start + width * idx);
      host->Set((host->Get() & ~mask) |
                ((static_cast<StorageType>(val) << (start + width * idx)) & mask));
    }

    constexpr View<FieldType> operator[](const std::size_t idx)
    {
      assert(idx < length);  // Index out of range
      return View<FieldType>(host, start + width * idx, width);
    }
    constexpr ConstView<FieldType> operator[](const std::size_t idx) const
    {
      assert(idx < length);  // Index out of range
      return ConstView<FieldType>(host, start + width * idx, width);
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

  private:
    BitField2<StorageType>* const host;
  };

  /////////////////////////////////////////////////////////////////////////////////////////////////

  template <typename FieldType, std::size_t start, std::size_t width, std::size_t length>
  struct ConstFixedViewArray
  {
    ConstFixedViewArray() = delete;
    constexpr ConstFixedViewArray(const BitField2<StorageType>* host_) : host(host_) {}

    constexpr static std::size_t rshift = 8 * sizeof(StorageType) - width;

    constexpr FieldType Get(const std::size_t idx) const
    {
      assert(idx < length);  // Index out of range
      const std::size_t lshift = rshift - (start + width * idx);
      // Choose between arithmatic (sign extend) or logical (no sign extend) right-shift.
      if (std::is_signed<FieldType>())
        return static_cast<FieldType>(static_cast<StorageTypeS>(host->Get() << lshift) >> rshift);
      else
        return static_cast<FieldType>(static_cast<StorageTypeU>(host->Get() << lshift) >> rshift);
    }

    ConstView<FieldType> operator[](const std::size_t idx) const
    {
      assert(idx < length);  // Index out of range
      return ConstView<FieldType>(host, start + width * idx, width);
    }

    using ConstIterator = FixedViewArrayConstIterator<FieldType, start, width>;

    constexpr ConstIterator begin() const { return ConstIterator(host, 0); }
    constexpr ConstIterator end() const { return ConstIterator(host, length); }
    constexpr ConstIterator cbegin() const { return ConstIterator(host, 0); }
    constexpr ConstIterator cend() const { return ConstIterator(host, length); }
    constexpr std::size_t Size() const { return length; }
    constexpr std::size_t Length() const { return length; }

  private:
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
  constexpr ConstFixedView<const FieldType, start, width> name() const                             \
  {                                                                                                \
    return ConstFixedView<const FieldType, start, width>(this);                                    \
  }
#define FIELD(FieldType, start, width, name)                                                       \
  constexpr FixedView<FieldType, start, width> name()                                              \
  {                                                                                                \
    return FixedView<FieldType, start, width>(this);                                               \
  }                                                                                                \
  CONSTFIELD(FieldType, start, width, name)
#define CONSTFIELDARRAY(FieldType, start, width, length, name)                                     \
  constexpr ConstFixedViewArray<const FieldType, start, width, length> name() const                \
  {                                                                                                \
    return ConstFixedViewArray<const FieldType, start, width, length>(this);                       \
  }
#define FIELDARRAY(FieldType, start, width, length, name)                                          \
  constexpr FixedViewArray<FieldType, start, width, length> name()                                 \
  {                                                                                                \
    return FixedViewArray<FieldType, start, width, length>(this);                                  \
  }                                                                                                \
  CONSTFIELDARRAY(FieldType, start, width, length, name)

// For composition-based polymorphism, give the ability to specify a host BitField.
#define CONSTFIELD_IN(host, FieldType, start, width, name)                                         \
  constexpr decltype(host)::ConstFixedView<const FieldType, start, width> name() const             \
  {                                                                                                \
    return decltype(host)::ConstFixedView<const FieldType, start, width>(&host);                   \
  }
#define FIELD_IN(host, FieldType, start, width, name)                                              \
  constexpr decltype(host)::FixedView<FieldType, start, width> name()                              \
  {                                                                                                \
    return decltype(host)::FixedView<FieldType, start, width>(&host);                              \
  }                                                                                                \
  CONSTFIELD_IN(host, FieldType, start, width, name)
#define CONSTFIELDARRAY_IN(host, FieldType, start, width, length, name)                            \
  constexpr decltype(host)::ConstFixedViewArray<const FieldType, start, width, length> name()      \
      const                                                                                        \
  {                                                                                                \
    return decltype(host)::ConstFixedViewArray<const FieldType, start, width, length>(&host);      \
  }
#define FIELDARRAY_IN(host, FieldType, start, width, length, name)                                 \
  constexpr decltype(host)::FixedViewArray<FieldType, start, width, length> name()                 \
  {                                                                                                \
    return decltype(host)::FixedViewArray<FieldType, start, width, length>(&host);                 \
  }                                                                                                \
  CONSTFIELDARRAY_IN(host, FieldType, start, width, length, name)

// TODO ConstFixedViewArray
