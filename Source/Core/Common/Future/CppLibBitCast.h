// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <version>

// The purpose of "future" headers is to backport syntactic-sugar C++ Standard Library features to
// compilers versions lacking said library features (e.g. Dolphin's buildbots).  Please replace
// usage of this header with the C++ Standard Library equivalent as soon as is possible.

#ifdef __cpp_lib_bit_cast
#include <bit>
#else
#include "Common/Concepts.h"
namespace std
{
#define __cpp_lib_bit_cast 201806L

template <Common::TriviallyCopyable To, Common::TriviallyCopyable From>
requires(sizeof(To) == sizeof(From)) [[nodiscard]] constexpr To
    bit_cast(const From& source) noexcept
{
  // GCC, Clang, and even MSVC all agree to name this "__builtin_bit_cast".
  return __builtin_bit_cast(To, source);
}
};  // namespace std
#endif
