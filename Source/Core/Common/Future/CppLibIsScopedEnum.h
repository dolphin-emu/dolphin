// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <type_traits>
#include <version>

// The purpose of "future" headers is to backport syntactic-sugar C++ Standard Library features to
// compilers versions lacking said library features (e.g. Dolphin's buildbots).  Please replace
// usage of this header with the C++ Standard Library equivalent as soon as is possible.

#ifndef __cpp_lib_is_scoped_enum
namespace std
{
#define __cpp_lib_is_scoped_enum 202011L

namespace detail
{
template <class T>
concept unscoped_enum = requires(T t, void (*f)(int))
{
  is_enum_v<T>;
  f(t);
};

template <class T>
concept complete_type = requires(T t)
{
  t = t;
};
}  // namespace detail

template <class T>
struct is_scoped_enum : false_type
{
};

template <detail::complete_type T>
struct is_scoped_enum<T> : bool_constant<!detail::unscoped_enum<std::remove_cv_t<T>>>
{
};

template <class T>
inline constexpr bool is_scoped_enum_v = is_scoped_enum<T>::value;
}  // namespace std
#endif
