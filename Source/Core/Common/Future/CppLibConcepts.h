// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <version>

// C++20 Standard Library support in the Android NDK is lacking.  However, the core language feature
// of concepts works fine in all environments Dolphin Emulator currently supports.  Please replace
// the usage of this header with the C++ Standard Library equivalent as soon as is possible.

#ifdef __cpp_lib_concepts
#include <concepts>
#else
#include <type_traits>
namespace std
{
// Technically inaccurate because we haven't defined *all* of the standard concepts
#define __cpp_lib_concepts 202002L

template <class T, class U>
concept same_as = std::is_same_v<T, U> && std::is_same_v<U, T>;

template <class T>
concept integral = std::is_integral_v<T>;

template <class T>
concept signed_integral = integral<T> && std::is_signed_v<T>;

template <class T>
concept unsigned_integral = integral<T> && std::is_unsigned_v<T>;

template <class T>
concept floating_point = std::is_floating_point_v<T>;

template <class Derived, class Base>
concept derived_from = std::is_base_of_v<Base, Derived> &&
    std::is_convertible_v<const volatile Derived*, const volatile Base*>;
};  // namespace std
#endif
