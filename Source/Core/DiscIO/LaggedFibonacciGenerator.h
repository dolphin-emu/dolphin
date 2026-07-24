// SPDX-License-Identifier: CC0-1.0

#pragma once

#include <array>
#include <cstddef>
#include <span>

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

  // Generates a disc-level junk seed from disc ID, disc number, and sector index.
  // Based on: https://github.com/encounter/nod/blob/d10d376/nod/src/util/lfg.rs#L62
  static void GenerateSeed(u32 seed_out[SEED_SIZE], const u8 disc_id[4], u8 disc_num, u32 sector);

  // Fills a buffer with the expected disc-level junk pattern.
  // Re-seeds per 0x8000-byte sector boundary.
  static void FillJunkData(std::span<u8> out, u64 disc_offset, const u8 disc_id[4], u8 disc_num);

  // Checks if a block of data matches the expected disc-level junk pattern.
  // Re-seeds per 0x8000-byte sector boundary. Returns true if the entire block is junk.
  static bool IsJunkBlock(const u8* data, size_t size, u64 disc_offset, const u8 disc_id[4],
                          u8 disc_num);

  // SetSeed must be called before using the functions below
  void SetSeed(const u32 seed[SEED_SIZE]);
  void SetSeed(const u8 seed[SEED_SIZE * sizeof(u32)]);

  // Outputs a number of bytes and advances the internal state by the same amount.
  void GetBytes(size_t count, u8* out);
  // Compares data against the LFG state, returns false at first mismatch.
  bool CompareBytes(size_t count, const u8* data);
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
