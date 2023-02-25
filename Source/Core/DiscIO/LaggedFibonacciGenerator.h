// SPDX-License-Identifier: CC0-1.0

#pragma once

#include <array>
#include <cstddef>

#include "Common/CommonTypes.h"

namespace DiscIO
{
class LaggedFibonacciGenerator
{
public:
  static constexpr size_t SEED_SIZE = 17;

  // Reconstructs a seed and writes it to seed_out, then returns the number of bytes which can
  // be reconstructed using that seed. Can return any number between 0 and size, inclusive.
  // data - data_offset must be 4-byte aligned.
  static size_t GetSeed(const u8* data, size_t size, size_t data_offset, u32 seed_out[SEED_SIZE]);

  // SetSeed must be called before using the functions below
  void SetSeed(const u32 seed[SEED_SIZE]);
  void SetSeed(const u8 seed[SEED_SIZE * sizeof(u32)]);

  // Outputs a number of bytes and advances the internal state by the same amount.
  void GetBytes(size_t count, u8* out);
  u8 GetByte();

  // Advances the internal state like GetBytes, but without outputting data. O(N), like GetBytes.
  void Forward(size_t count);

private:
  static bool GetSeed(const u32* data, size_t size, size_t data_offset,
                      LaggedFibonacciGenerator* lfg, u32 seed_out[SEED_SIZE]);

  void Forward();
  void Backward(size_t start_word = 0, size_t end_word = LFG_K);

  bool Reinitialize(u32 seed_out[SEED_SIZE]);
  bool Initialize(bool check_existing_data);

  static constexpr size_t LFG_K = 521;
  static constexpr size_t LFG_J = 32;

  std::array<u32, LFG_K> m_buffer{};

  size_t m_position_bytes = 0;
};

}  // namespace DiscIO
