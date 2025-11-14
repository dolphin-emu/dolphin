// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cmath>
#include <concepts>
#include <limits>
#include <numeric>
#include <utility>

#include "Common/MathUtil.h"

namespace MathUtil
{
template <std::integral T>
class Rational;

namespace detail
{
template <typename T>
concept RationalCompatible = requires(T x) { Rational(x); };

template <typename T>
concept RationalAdjacent = std::is_arithmetic_v<T>;

// Allows for class template argument deduction within the class body.
// Note: CTAD via alias template would be cleaner but was not implemented until Clang 19.
template <typename Other>
constexpr auto DeducedRational(Other other)
{
  return Rational{other};
}
}  // namespace detail

// Rational number class template
// Results are not currently automatically reduced.
// Watch out for integer overflow after repeated operations.
// Use Reduced / Approximated functions as needed to avoid overflow.
template <std::integral T>
class Rational final
{
public:
  using ValueType = T;

  ValueType numerator{0};
  ValueType denominator{1};

  // Conversion from integers
  constexpr Rational(T num = 0, T den = 1) : numerator{num}, denominator{den} {}

  // Conversion from floating point
  // Properly converts nice values like 0.5f -> Rational(1, 2)
  // Inexact values like 0.3f won't convert particularly nicely.
  // But e.g. Rational(0.3f).Approximated(10) will produce Rational(3, 10)
  constexpr explicit Rational(std::floating_point auto float_value)
  {
    constexpr int dest_exp = IntLog2(std::numeric_limits<T>::max()) - 1;
    int exp = 0;
    const auto norm_frac = std::frexp(float_value, &exp);

    const int num_exp = std::max(exp, dest_exp);
    numerator = SaturatingCast<T>(std::ldexp(norm_frac, num_exp));
    const int zeros = std::countr_zero(std::make_unsigned_t<T>(numerator));

    const int den_exp = num_exp - exp;
    const int shift_right = std::min(den_exp, zeros);
    denominator = SaturatingCast<T>(std::ldexp(1, den_exp - shift_right));

    numerator >>= shift_right;
  }

  // Conversion from other Rational.
  template <std::integral Other>
  constexpr explicit Rational(Rational<Other> other)
  {
    if constexpr (std::is_unsigned_v<T> && std::is_signed_v<Other>)
      other = other.Normalized();
    numerator = SaturatingCast<T>(other.numerator);
    denominator = SaturatingCast<T>(other.denominator);
  }

  // Potentially lossy conversion to int/float types
  template <detail::RationalAdjacent Target>
  constexpr explicit operator Target() const
  {
    return Target(numerator) / Target(denominator);
  }

  constexpr bool IsInteger() const { return (numerator % denominator) == 0; }

  constexpr Rational Inverted() const { return Rational(denominator, numerator); }

  // Returns a copy with a non-negative denominator.
  constexpr Rational Normalized() const
  {
    return (denominator < T{0}) ? Rational(T{} - numerator, T{} - denominator) : *this;
  }

  // Returns a reduced fraction.
  constexpr Rational Reduced() const
  {
    const auto gcd = std::gcd(numerator, denominator);
    return {T(numerator / gcd), T(denominator / gcd)};
  }

  // Returns a reduced approximated fraction with a given maximum numerator/denominator.
  constexpr Rational Approximated(T max_num_den) const
  {
    // This algorithm comes from FFmpeg's av_reduce function (LGPLv2.1+)

    const auto reduced_normalized = Reduced().Normalized();
    auto [num, den] = reduced_normalized;

    const bool is_negative = num < T{0};
    if (is_negative)
      num *= -1;

    if (num <= max_num_den && den <= max_num_den)
      return reduced_normalized;

    Rational a0{0, 1};
    Rational a1{1, 0};

    while (den != 0)
    {
      auto x = num / den;
      const auto next_den = num - (den * x);
      const Rational a2 = (Rational(x, x) * a1) & a0;

      if (a2.numerator > max_num_den || a2.denominator > max_num_den)
      {
        if (a1.numerator != 0)
          x = (max_num_den - a0.numerator) / a1.numerator;

        if (a1.denominator != 0)
          x = std::min(x, (max_num_den - a0.denominator) / a1.denominator);

        if (den * (2 * x * a1.denominator + a0.denominator) > num * a1.denominator)
          a1 = {(Rational(x, x) * a1) & a0};
        break;
      }

      a0 = a1;
      a1 = a2;
      num = den;
      den = next_den;
    }

    return is_negative ? -a1 : a1;
  }

  // Multiplication
  constexpr auto& operator*=(detail::RationalCompatible auto rhs)
  {
    const auto r = CommonRational(rhs);
    numerator *= r.numerator;
    denominator *= r.denominator;
    return *this;
  }
  constexpr friend auto operator*(detail::RationalCompatible auto lhs, Rational rhs)
  {
    return CommonRational(lhs) *= rhs;
  }
  constexpr friend auto operator*(Rational lhs, detail::RationalAdjacent auto rhs)
  {
    return lhs * CommonRational(rhs);
  }

  // Division
  constexpr auto& operator/=(detail::RationalCompatible auto rhs)
  {
    return *this *= CommonRational(rhs).Inverted();
  }
  constexpr friend auto operator/(detail::RationalCompatible auto lhs, Rational rhs)
  {
    return CommonRational(lhs) *= rhs.Inverted();
  }
  constexpr friend auto operator/(Rational lhs, detail::RationalAdjacent auto rhs)
  {
    return lhs * CommonRational(rhs).Inverted();
  }

  // Modulo (behaves like fmod)
  constexpr auto& operator%=(detail::RationalCompatible auto rhs)
  {
    const auto r = CommonRational(rhs);
    return *this -= (r * T(*this / r));
  }
  constexpr friend auto operator%(detail::RationalCompatible auto lhs, Rational rhs)
  {
    return CommonRational(lhs) %= rhs;
  }
  constexpr friend auto operator%(Rational lhs, detail::RationalAdjacent auto rhs)
  {
    return lhs % CommonRational(rhs);
  }

  // Addition
  constexpr auto& operator+=(detail::RationalCompatible auto rhs)
  {
    const auto r = CommonRational(rhs);
    numerator *= r.denominator;
    numerator += r.numerator * denominator;
    denominator *= r.denominator;
    return *this;
  }
  constexpr friend auto operator+(detail::RationalCompatible auto lhs, Rational rhs)
  {
    return CommonRational(lhs) += rhs;
  }
  constexpr friend auto operator+(Rational lhs, detail::RationalAdjacent auto rhs)
  {
    return lhs + CommonRational(rhs);
  }

  // Subtraction
  constexpr auto& operator-=(detail::RationalCompatible auto rhs)
  {
    const auto r = CommonRational(rhs);
    numerator *= r.denominator;
    numerator -= r.numerator * denominator;
    denominator *= r.denominator;
    return *this;
  }
  constexpr friend auto operator-(detail::RationalCompatible auto lhs, Rational rhs)
  {
    return CommonRational(lhs) -= rhs;
  }
  constexpr friend auto operator-(Rational lhs, detail::RationalAdjacent auto rhs)
  {
    return lhs - CommonRational(rhs);
  }

  // Mediant (n1+n2)/(d1+d2)
  constexpr auto& operator&=(detail::RationalCompatible auto rhs)
  {
    const auto r = CommonRational(rhs);
    numerator += r.numerator;
    denominator += r.denominator;
    return *this;
  }
  constexpr friend auto operator&(detail::RationalCompatible auto lhs, Rational rhs)
  {
    return CommonRational(lhs) &= rhs;
  }
  constexpr friend auto operator&(Rational lhs, detail::RationalAdjacent auto rhs)
  {
    return lhs & CommonRational(rhs);
  }

  // Comparison
  constexpr friend auto operator<=>(detail::RationalCompatible auto lhs, Rational rhs)
  {
    const auto left = detail::DeducedRational(lhs).Normalized();
    const auto lhs_q = left.numerator / left.denominator;
    const auto lhs_r = left.numerator % left.denominator;

    rhs = rhs.Normalized();
    const auto rhs_q = rhs.numerator / rhs.denominator;
    const auto rhs_r = rhs.numerator % rhs.denominator;

    // If integer division results differ we have a result.
    if (const auto cmp = Compare3Way(lhs_q, rhs_q); std::is_neq(cmp))
      return cmp;

    // If at least one side has no remainder we have a result.
    if (lhs_r == 0 || rhs_r == 0)
      return Compare3Way(lhs_r, rhs_r);

    // Recurse with inverted remainders.
    return Rational(rhs.denominator, rhs_r) <=> decltype(left)(left.denominator, lhs_r);
  }
  constexpr friend auto operator<=>(Rational lhs, detail::RationalAdjacent auto rhs)
  {
    return lhs <=> detail::DeducedRational(rhs);
  }

  // Equality
  constexpr friend bool operator==(detail::RationalCompatible auto lhs, Rational rhs)
  {
    return std::is_eq(lhs <=> rhs);
  }
  constexpr friend bool operator==(Rational lhs, detail::RationalAdjacent auto rhs)
  {
    return std::is_eq(lhs <=> rhs);
  }

  // Unary operators
  constexpr auto operator+() const { return *this; }
  constexpr auto operator-() const { return Rational(T{} - numerator, denominator); }
  constexpr auto& operator++() { return *this += 1; }
  constexpr auto& operator--() { return *this -= 1; }
  constexpr auto operator++(int) { return std::exchange(*this, *this + 1); }
  constexpr auto operator--(int) { return std::exchange(*this, *this - 1); }

  constexpr explicit operator bool() const { return numerator != 0; }

private:
  // Constructs a common_type'd Rational<T> from an existing value.
  static constexpr auto CommonRational(auto val)
  {
    return Rational<
        std::common_type_t<T, typename decltype(detail::DeducedRational(val))::ValueType>>(val);
  }
};

// Floating point deduction guides.
Rational(float) -> Rational<s32>;
Rational(double) -> Rational<s64>;

}  // namespace MathUtil
