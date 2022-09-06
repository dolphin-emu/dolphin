// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <memory>

#include "Common/CommonTypes.h"
#include "Common/Concepts.h"

namespace Common::Random
{
/// Fill `buffer` with random bytes using a cryptographically secure pseudo-random number generator.
void Generate(void* buffer, std::size_t size);

/// Generates a random value of arithmetic type `T`
template <Arithmetic T>
T GenerateValue()
{
  T value;
  Generate(&value, sizeof(value));
  return value;
}
}  // namespace Common::Random
