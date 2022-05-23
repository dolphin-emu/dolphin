#pragma once

#include <cassert>
#include <cstddef>
#include <iterator>
#include <limits>
#include <type_traits>

///////////////////////////////////////////////////////////////////////////////////////////////////

template <typename host_t_, typename field_t_, std::size_t start_, std::size_t width_>
class BitFieldFixedView
{
  // This code is only sane for integral types
  static_assert(std::is_integral_v<host_t_> || std::is_enum_v<host_t_>);
  static_assert(std::is_integral_v<field_t_> || std::is_enum_v<field_t_>);

public:
  using host_t = host_t_;
  using field_t = field_t_;
  constexpr static std::size_t start = start_;
  constexpr static std::size_t width = width_;

private:
  using uhost_t = std::make_unsigned_t<host_t>;
  using shost_t = std::make_signed_t<host_t>;
  constexpr static std::size_t rshift = 8 * sizeof(host_t) - width;
  constexpr static std::size_t lshift = rshift - start;
  host_t& host;

public:
  BitFieldFixedView() = delete;
  constexpr BitFieldFixedView(host_t& host_) : host(host_){};

  constexpr operator field_t() const { return Get(); }
  constexpr BitFieldFixedView& operator=(const BitFieldFixedView& rhs)
  {
    return operator=(rhs.Get());
  }

  constexpr field_t Get() const
  {
    // Choose between arithmatic (sign extend) or logical (no sign extend) right-shift.
    if constexpr (std::is_signed_v<field_t>)
      return static_cast<field_t>(static_cast<shost_t>(host << lshift) >> rshift);
    else
      return static_cast<field_t>(static_cast<uhost_t>(host << lshift) >> rshift);
  }

  constexpr void Set(const field_t val)
  {
    constexpr uhost_t mask = std::numeric_limits<uhost_t>::max() >> rshift << start;
    host = (host & ~mask) | ((static_cast<host_t>(val) << start) & mask);
  }

  constexpr BitFieldFixedView& operator=(const field_t rhs)
  {
    Set(rhs);
    return *this;
  }
  constexpr BitFieldFixedView& operator+=(const field_t rhs) { return operator=(Get() + rhs); }
  constexpr BitFieldFixedView& operator-=(const field_t rhs) { return operator=(Get() - rhs); }
  constexpr BitFieldFixedView& operator*=(const field_t rhs) { return operator=(Get() * rhs); }
  constexpr BitFieldFixedView& operator/=(const field_t rhs) { return operator=(Get() / rhs); }
  constexpr BitFieldFixedView& operator|=(const field_t rhs) { return operator=(Get() / rhs); }
  constexpr BitFieldFixedView& operator&=(const field_t rhs) { return operator=(Get() & rhs); }
  constexpr BitFieldFixedView& operator^=(const field_t rhs) { return operator=(Get() ^ rhs); }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <typename host_t_, typename field_t_, std::size_t start_, std::size_t width_>
class BitFieldConstFixedView
{
  // This code is only sane for integral types
  static_assert(std::is_integral_v<host_t_> || std::is_enum_v<host_t_>);
  static_assert(std::is_integral_v<field_t_> || std::is_enum_v<field_t_>);

public:
  using host_t = host_t_;
  using field_t = field_t_;
  constexpr static std::size_t start = start_;
  constexpr static std::size_t width = width_;

private:
  using uhost_t = std::make_unsigned_t<host_t>;
  using shost_t = std::make_signed_t<host_t>;
  constexpr static std::size_t rshift = 8 * sizeof(host_t) - width;
  constexpr static std::size_t lshift = rshift - start;
  const host_t& host;

public:
  BitFieldConstFixedView() = delete;
  constexpr BitFieldConstFixedView(const host_t& host_) : host(host_){};

  constexpr operator field_t() const { return Get(); }

  constexpr field_t Get() const
  {
    // Choose between arithmatic (sign extend) or logical (no sign extend) right-shift.
    if constexpr (std::is_signed_v<field_t>)
      return static_cast<field_t>(static_cast<shost_t>(host << lshift) >> rshift);
    else
      return static_cast<field_t>(static_cast<uhost_t>(host << lshift) >> rshift);
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <typename host_t_, typename field_t_>
class BitFieldView
{
  // This code is only sane for integral types
  static_assert(std::is_integral_v<host_t_> || std::is_enum_v<host_t_>);
  static_assert(std::is_integral_v<field_t_> || std::is_enum_v<field_t_>);

public:
  using host_t = host_t_;
  using field_t = field_t_;

private:
  using uhost_t = std::make_unsigned_t<host_t>;
  using shost_t = std::make_signed_t<host_t>;
  host_t& host;
  const std::size_t start;
  const std::size_t width;

public:
  BitFieldView() = delete;
  constexpr BitFieldView(host_t& host_, const std::size_t start_, const std::size_t width_)
      : host(host_), start(start_), width(width_){};

  constexpr operator field_t() const { return Get(); }
  constexpr BitFieldView& operator=(const BitFieldView& rhs) { return operator=(rhs.Get()); }

  constexpr field_t Get() const
  {
    const std::size_t rshift = 8 * sizeof(host_t) - width;
    const std::size_t lshift = rshift - start;
    // Choose between arithmatic (sign extend) or logical (no sign extend) right-shift.
    if constexpr (std::is_signed<field_t>())
      return static_cast<field_t>(static_cast<shost_t>(host << lshift) >> rshift);
    else
      return static_cast<field_t>(static_cast<uhost_t>(host << lshift) >> rshift);
  }

  constexpr void Set(const field_t val)
  {
    const std::size_t rshift = 8 * sizeof(host_t) - width;
    const uhost_t mask = std::numeric_limits<uhost_t>::max() >> rshift << start;
    host = (host & ~mask) | ((static_cast<host_t>(val) << start) & mask);
  }

  constexpr BitFieldView& operator=(const field_t rhs)
  {
    Set(rhs);
    return *this;
  }
  constexpr BitFieldView& operator+=(const field_t rhs) { return operator=(Get() + rhs); }
  constexpr BitFieldView& operator-=(const field_t rhs) { return operator=(Get() - rhs); }
  constexpr BitFieldView& operator*=(const field_t rhs) { return operator=(Get() * rhs); }
  constexpr BitFieldView& operator/=(const field_t rhs) { return operator=(Get() / rhs); }
  constexpr BitFieldView& operator|=(const field_t rhs) { return operator=(Get() | rhs); }
  constexpr BitFieldView& operator&=(const field_t rhs) { return operator=(Get() & rhs); }
  constexpr BitFieldView& operator^=(const field_t rhs) { return operator=(Get() ^ rhs); }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <typename host_t_, typename field_t_>
class BitFieldConstView
{
  // This code is only sane for integral types
  static_assert(std::is_integral_v<host_t_> || std::is_enum_v<host_t_>);
  static_assert(std::is_integral_v<field_t_> || std::is_enum_v<field_t_>);

public:
  using host_t = host_t_;
  using field_t = field_t_;

private:
  using uhost_t = std::make_unsigned_t<host_t>;
  using shost_t = std::make_signed_t<host_t>;
  const host_t& host;
  const std::size_t start;
  const std::size_t width;

public:
  BitFieldConstView() = delete;
  constexpr BitFieldConstView(const host_t& host_, const std::size_t start_,
                              const std::size_t width_)
      : host(host_), start(start_), width(width_){};

  constexpr operator field_t() const { return Get(); }

  constexpr const field_t Get() const
  {
    const std::size_t rshift = 8 * sizeof(host_t) - width;
    const std::size_t lshift = rshift - start;
    // Choose between arithmatic (sign extend) or logical (no sign extend) right-shift.
    if constexpr (std::is_signed<field_t>())
      return static_cast<field_t>(static_cast<shost_t>(host << lshift) >> rshift);
    else
      return static_cast<field_t>(static_cast<uhost_t>(host << lshift) >> rshift);
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <typename host_t_, typename field_t_, std::size_t start_, std::size_t width_>
struct FixedViewArrayIterator;
template <typename host_t_, typename field_t_, std::size_t start_, std::size_t width_>
struct FixedViewArrayConstIterator;

///////////////////////////////////////////////////////////////////////////////////////////////////

template <typename host_t_, typename field_t_, std::size_t start_, std::size_t width_,
          std::size_t length_>
class FixedViewArray
{
  // This code is only sane for integral types
  static_assert(std::is_integral_v<host_t_> || std::is_enum_v<host_t_>);
  static_assert(std::is_integral_v<field_t_> || std::is_enum_v<field_t_>);

public:
  using host_t = host_t_;
  using field_t = field_t_;
  constexpr static std::size_t start = start_;
  constexpr static std::size_t width = width_;
  constexpr static std::size_t length = length_;

private:
  using uhost_t = std::make_unsigned_t<host_t>;
  using shost_t = std::make_signed_t<host_t>;
  using Iterator = FixedViewArrayIterator<host_t, field_t, start, width>;
  using ConstIterator = FixedViewArrayConstIterator<host_t, field_t, start, width>;
  constexpr static std::size_t rshift = 8 * sizeof(host_t) - width;
  host_t& host;

public:
  FixedViewArray() = delete;
  constexpr FixedViewArray(host_t& host_) : host(host_) {}

  constexpr field_t Get(const std::size_t idx) const
  {
    assert(idx < length);  // Index out of range
    const std::size_t lshift = rshift - (start + width * idx);
    // Choose between arithmatic (sign extend) or logical (no sign extend) right-shift.
    if constexpr (std::is_signed<field_t>())
      return static_cast<field_t>(static_cast<shost_t>(host << lshift) >> rshift);
    else
      return static_cast<field_t>(static_cast<uhost_t>(host << lshift) >> rshift);
  }
  constexpr void Set(const std::size_t idx, const field_t val)
  {
    assert(idx < length);  // Index out of range
    const uhost_t mask = (std::numeric_limits<uhost_t>::max() >> rshift) << (start + width * idx);
    host = (host & ~mask) | ((static_cast<host_t>(val) << (start + width * idx)) & mask);
  }

  constexpr BitFieldView<host_t, field_t> operator[](const std::size_t idx)
  {
    assert(idx < length);  // Index out of range
    return BitFieldView<host_t, field_t>(host, start + width * idx, width);
  }
  constexpr BitFieldConstView<host_t, field_t> operator[](const std::size_t idx) const
  {
    assert(idx < length);  // Index out of range
    return BitFieldConstView<host_t, field_t>(host, start + width * idx, width);
  }

  constexpr Iterator begin() { return Iterator(host, 0); }
  constexpr Iterator end() { return Iterator(host, length); }
  constexpr ConstIterator begin() const { return ConstIterator(host, 0); }
  constexpr ConstIterator end() const { return ConstIterator(host, length); }
  constexpr ConstIterator cbegin() const { return ConstIterator(host, 0); }
  constexpr ConstIterator cend() const { return ConstIterator(host, length); }
  constexpr std::size_t Size() const { return length; }
  constexpr std::size_t Length() const { return length; }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <typename host_t_, typename field_t_, std::size_t start_, std::size_t width_,
          std::size_t length_>
class ConstFixedViewArray
{
  // This code is only sane for integral types
  static_assert(std::is_integral_v<host_t_> || std::is_enum_v<host_t_>);
  static_assert(std::is_integral_v<field_t_> || std::is_enum_v<field_t_>);

public:
  using host_t = host_t_;
  using field_t = field_t_;
  constexpr static std::size_t start = start_;
  constexpr static std::size_t width = width_;
  constexpr static std::size_t length = length_;

private:
  using uhost_t = std::make_unsigned_t<host_t>;
  using shost_t = std::make_signed_t<host_t>;
  using ConstIterator = FixedViewArrayConstIterator<host_t, field_t, start, width>;
  constexpr static std::size_t rshift = 8 * sizeof(host_t) - width;
  const host_t& host;

public:
  ConstFixedViewArray() = delete;
  constexpr ConstFixedViewArray(const host_t& host_) : host(host_) {}

  constexpr field_t Get(const std::size_t idx) const
  {
    assert(idx < length);  // Index out of range
    const std::size_t lshift = rshift - (start + width * idx);
    // Choose between arithmatic (sign extend) or logical (no sign extend) right-shift.
    if constexpr (std::is_signed<field_t>())
      return static_cast<field_t>(static_cast<shost_t>(host << lshift) >> rshift);
    else
      return static_cast<field_t>(static_cast<uhost_t>(host << lshift) >> rshift);
  }

  BitFieldConstView<host_t, field_t> operator[](const std::size_t idx) const
  {
    assert(idx < length);  // Index out of range
    return BitFieldConstView<host_t, field_t>(host, start + width * idx, width);
  }

  constexpr ConstIterator begin() const { return ConstIterator(host, 0); }
  constexpr ConstIterator end() const { return ConstIterator(host, length); }
  constexpr ConstIterator cbegin() const { return ConstIterator(host, 0); }
  constexpr ConstIterator cend() const { return ConstIterator(host, length); }
  constexpr std::size_t Size() const { return length; }
  constexpr std::size_t Length() const { return length; }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <typename host_t_, typename field_t_, std::size_t start_, std::size_t width_>
class FixedViewArrayIterator
{
  // This code is only sane for integral types
  static_assert(std::is_integral_v<host_t_> || std::is_enum_v<host_t_>);
  static_assert(std::is_integral_v<field_t_> || std::is_enum_v<field_t_>);

public:
  using host_t = host_t_;
  using field_t = field_t_;
  using iterator_category = std::output_iterator_tag;
  using value_type = field_t;
  using difference_type = ptrdiff_t;
  using pointer = void;
  using reference = BitFieldView<host_t, field_t>;
  constexpr static std::size_t start = start_;
  constexpr static std::size_t width = width_;

private:
  host_t& host;
  std::size_t idx;

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

  FixedViewArrayIterator(host_t& host_, const std::size_t idx_) : host(host_), idx(idx_) {}

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
  constexpr reference operator*() const { return reference(host, start + width * idx, width); }
  constexpr bool operator==(const FixedViewArrayIterator& other) const { return idx == other.idx; }
  constexpr bool operator!=(const FixedViewArrayIterator& other) const { return idx != other.idx; }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <typename host_t_, typename field_t_, std::size_t start_, std::size_t width_>
struct FixedViewArrayConstIterator
{
  // This code is only sane for integral types
  static_assert(std::is_integral_v<host_t_> || std::is_enum_v<host_t_>);
  static_assert(std::is_integral_v<field_t_> || std::is_enum_v<field_t_>);

public:
  using host_t = host_t_;
  using field_t = field_t_;
  using iterator_category = std::input_iterator_tag;
  using value_type = field_t;
  using difference_type = ptrdiff_t;
  using pointer = void;
  using reference = BitFieldConstView<host_t, field_t>;
  constexpr static std::size_t start = start_;
  constexpr static std::size_t width = width_;

private:
  const host_t& host;
  std::size_t idx;

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

  FixedViewArrayConstIterator(const host_t& host_, const std::size_t idx_) : host(host_), idx(idx_)
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
  constexpr reference operator*() const { return reference(host, start + width * idx, width); }
  constexpr bool operator==(const FixedViewArrayConstIterator other) const
  {
    return idx == other.idx;
  }
  constexpr bool operator!=(const FixedViewArrayConstIterator other) const
  {
    return idx != other.idx;
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

// Mix up the template argument order on purpose to allow implicit deduction of host_t.  Even C++17
// cannot deduce trailing template parameters for constructors.  For them, it's all or nothing.
// Functions, however, can.

template <typename field_t, const std::size_t start, const std::size_t width, typename host_t>
constexpr BitFieldFixedView<host_t, field_t, start, width>  //
MakeBitFieldFixedView(host_t& host)
{
  return BitFieldFixedView<host_t, field_t, start, width>(host);
}
template <typename field_t, const std::size_t start, const std::size_t width, typename host_t>
constexpr BitFieldConstFixedView<host_t, field_t, start, width>
MakeBitFieldConstFixedView(const host_t& host)
{
  return BitFieldConstFixedView<host_t, field_t, start, width>(host);
}
template <typename field_t, typename host_t>
constexpr BitFieldView<host_t, field_t> MakeBitfieldView(host_t& host, const std::size_t start,
                                                         const std::size_t width)
{
  return BitFieldView<host_t, field_t>(host, start, width);
}
template <typename field_t, typename host_t>
constexpr BitFieldView<host_t, field_t>
MakeBitFieldConstView(const host_t& host, const std::size_t start, const std::size_t width)
{
  return BitFieldView<host_t, field_t>(host, start, width);
}
template <typename field_t, const std::size_t start, const std::size_t width,
          const std::size_t length, typename host_t>
constexpr FixedViewArray<host_t, field_t, start, width, length>
MakeBitFieldFixedViewArray(host_t& host)
{
  return FixedViewArray<host_t, field_t, start, width, length>(host);
}
template <typename field_t, const std::size_t start, const std::size_t width,
          const std::size_t length, typename host_t>
constexpr ConstFixedViewArray<host_t, field_t, start, width, length>
MakeBitFieldConstFixedViewArray(const host_t& host)
{
  return ConstFixedViewArray<host_t, field_t, start, width, length>(host);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

#define BITFIELD_IN(host, field_t, start, width, name)                                             \
  constexpr BitFieldFixedView<decltype(host), field_t, start, width> name()                        \
  {                                                                                                \
    return BitFieldFixedView<decltype(host), field_t, start, width>(host);                         \
  }                                                                                                \
  constexpr BitFieldConstFixedView<decltype(host), field_t, start, width> name() const             \
  {                                                                                                \
    return BitFieldConstFixedView<decltype(host), field_t, start, width>(host);                    \
  }

#define BITFIELDARRAY_IN(host, field_t, start, width, length, name)                                \
  constexpr FixedViewArray<decltype(host), field_t, start, width, length> name()                   \
  {                                                                                                \
    return FixedViewArray<decltype(host), field_t, start, width, length>(host);                    \
  }                                                                                                \
  constexpr ConstFixedViewArray<decltype(host), field_t, start, width, length> name() const        \
  {                                                                                                \
    return ConstFixedViewArray<decltype(host), field_t, start, width, length>(host);               \
  }

///////////////////////////////////////////////////////////////////////////////////////////////////

#include <fmt/format.h>

template <typename host_t, typename field_t, std::size_t start, std::size_t width>
struct fmt::formatter<BitFieldFixedView<host_t, field_t, start, width>>
{
  fmt::formatter<field_t> m_formatter;
  constexpr auto parse(format_parse_context& ctx) { return m_formatter.parse(ctx); }
  template <typename FormatContext>
  auto format(const BitFieldFixedView<host_t, field_t, start, width>& ref, FormatContext& ctx) const
  {
    return m_formatter.format(ref.Get(), ctx);
  }
};

template <typename host_t, typename field_t, std::size_t start, std::size_t width>
struct fmt::formatter<BitFieldConstFixedView<host_t, field_t, start, width>>
{
  fmt::formatter<field_t> m_formatter;
  constexpr auto parse(format_parse_context& ctx) { return m_formatter.parse(ctx); }
  template <typename FormatContext>
  auto format(const BitFieldConstFixedView<host_t, field_t, start, width>& ref,
              FormatContext& ctx) const
  {
    return m_formatter.format(ref.Get(), ctx);
  }
};

template <typename host_t, typename field_t>
struct fmt::formatter<BitFieldView<host_t, field_t>>
{
  fmt::formatter<field_t> m_formatter;
  constexpr auto parse(format_parse_context& ctx) { return m_formatter.parse(ctx); }
  template <typename FormatContext>
  auto format(const BitFieldView<host_t, field_t>& ref, FormatContext& ctx) const
  {
    return m_formatter.format(ref.Get(), ctx);
  }
};

template <typename host_t, typename field_t>
struct fmt::formatter<BitFieldConstView<host_t, field_t>>
{
  fmt::formatter<field_t> m_formatter;
  constexpr auto parse(format_parse_context& ctx) { return m_formatter.parse(ctx); }
  template <typename FormatContext>
  auto format(const BitFieldConstView<host_t, field_t>& ref, FormatContext& ctx) const
  {
    return m_formatter.format(ref.Get(), ctx);
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
