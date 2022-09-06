// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <limits>
#include <type_traits>

namespace Common
{
// These concepts are a terrible workaround for compilers (including some of Dolphin's buildbots)
// which support C++20 but have an outdated C++ stdlib.  Remove these redundant concepts ASAP!

// std::same_as
template <class T, class U>
concept SameAs = std::is_same_v<T, U> && std::is_same_v<U, T>;

// std::integral
template <class T>
concept Integral = std::is_integral_v<T>;

// std::signed_integral
template <class T>
concept SignedIntegral = Integral<T> && std::is_signed_v<T>;

// std::unsigned_integral
template <class T>
concept UnsignedIntegral = Integral<T> && std::is_unsigned_v<T>;

// std::floating_point
template <class T>
concept FloatingPoint = std::is_floating_point_v<T>;

// std::derived_from
template <class Derived, class Base>
concept DerivedFrom = std::is_base_of_v<Base, Derived> &&
    std::is_convertible_v<const volatile Derived*, const volatile Base*>;

// These other concepts are all new, though.  Keep these!
template <class T, class... U>
concept IsAnyOf = (SameAs<T, U> || ...);

template <class T>
concept TriviallyCopyable = std::is_trivially_copyable_v<T>;

template <class T>
concept NotTriviallyCopyable = !TriviallyCopyable<T>;

template <class T>
concept Enumerated = std::is_enum_v<T>;

template <class T>
concept NotEnumerated = !Enumerated<T>;

template <class T>
concept Class = std::is_class_v<T>;

template <class T>
concept Union = std::is_union_v<T>;

template <class T>
concept Arithmetic = std::is_arithmetic_v<T>;

template <class T>
concept Pointer = std::is_pointer_v<T>;

template <class T>
concept Function = std::is_function_v<T>;

template <class T>
concept FunctionPointer = Function<std::remove_pointer_t<T>>;

template <class T>
concept IntegralOrEnum = Integral<T> || Enumerated<T>;

template <class T, class U>
concept SameAsOrUnderlying = SameAs<T, U> || SameAs<std::underlying_type_t<T>, U>;
};  // namespace Common
