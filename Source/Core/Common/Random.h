// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <memory>
#include <type_traits>

#include "Common/CommonTypes.h"

namespace Common::Random
{
/// Fill `buffer` with random bytes using a cryptographically secure pseudo-random number generator.
void Generate(void* buffer, std::size_t size);

/// Generates a random value of arithmetic type `T`
template <typename T>
T GenerateValue()
{
  static_assert(std::is_arithmetic<T>(), "T must be an arithmetic type in GenerateValue.");
  T value;
  Generate(&value, sizeof(value));
  return value;
}
}  // namespace Common::Random
