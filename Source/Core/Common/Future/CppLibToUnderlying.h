// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <type_traits>
#include <utility>

// The purpose of "future" headers is to backport syntactic-sugar C++ Standard Library features to
// compilers versions lacking said library features (e.g. Dolphin's buildbots).  Please replace
// usage of this header with the C++ Standard Library equivalent as soon as is possible.

#ifndef __cpp_lib_to_underlying
namespace std
{
#define __cpp_lib_to_underlying 202102L
template <typename T>
[[nodiscard]] constexpr std::underlying_type_t<T> to_underlying(T val) noexcept
{
  return static_cast<std::underlying_type_t<T>>(val);
}
}  // namespace std
#endif
