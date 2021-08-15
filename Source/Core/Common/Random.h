// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <memory>
#include <type_traits>

#include "Common/CommonTypes.h"

namespace Common::Random
{
/// Cryptographically secure pseudo-random number generator, with explicit seed.
class PRNG final
{
public:
  explicit PRNG(u64 seed) : PRNG(&seed, sizeof(u64)) {}
  PRNG(void* seed, std::size_t size);
  ~PRNG();

  void Generate(void* buffer, std::size_t size);

  template <typename T>
  T GenerateValue()
  {
    static_assert(std::is_arithmetic<T>(), "T must be an arithmetic type in GenerateValue.");
    T value;
    Generate(&value, sizeof(value));
    return value;
  }

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

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
