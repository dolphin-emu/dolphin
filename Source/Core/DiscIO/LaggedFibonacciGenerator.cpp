// SPDX-License-Identifier: CC0-1.0

#include "DiscIO/LaggedFibonacciGenerator.h"

#include <algorithm>
#include <cstddef>
#include <cstring>

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Swap.h"

namespace DiscIO
{
void LaggedFibonacciGenerator::SetSeed(const u32 seed[SEED_SIZE])
{
  SetSeed(reinterpret_cast<const u8*>(seed));
}

void LaggedFibonacciGenerator::SetSeed(const u8 seed[SEED_SIZE * sizeof(u32)])
{
  m_position_bytes = 0;

  for (size_t i = 0; i < SEED_SIZE; ++i)
    m_buffer[i] = Common::swap32(seed + i * sizeof(u32));

  Initialize(false);
}

size_t LaggedFibonacciGenerator::GetSeed(const u8* data, size_t size, size_t data_offset,
                                         u32 seed_out[SEED_SIZE])
{
  if ((reinterpret_cast<uintptr_t>(data) - data_offset) % alignof(u32) != 0)
  {
    ASSERT(false);
    return 0;
  }

  // For code simplicity, only include whole u32 words when regenerating the seed. It would be
  // possible to get rid of this restriction and use a few additional bytes, but it's probably more
  // effort than it's worth considering that junk data often starts or ends on 4-byte offsets.
  const size_t bytes_to_skip = Common::AlignUp(data_offset, sizeof(u32)) - data_offset;
  const u32* u32_data = reinterpret_cast<const u32*>(data + bytes_to_skip);
  const size_t u32_size = (size - bytes_to_skip) / sizeof(u32);
  const size_t u32_data_offset = (data_offset + bytes_to_skip) / sizeof(u32);

  LaggedFibonacciGenerator lfg;
  if (!GetSeed(u32_data, u32_size, u32_data_offset, &lfg, seed_out))
    return false;

  lfg.m_position_bytes = data_offset % (LFG_K * sizeof(u32));

  const u8* end = data + size;
  size_t reconstructed_bytes = 0;
  while (data < end && lfg.GetByte() == *data)
  {
    ++reconstructed_bytes;
    ++data;
  }
  return reconstructed_bytes;
}

bool LaggedFibonacciGenerator::GetSeed(const u32* data, size_t size, size_t data_offset,
                                       LaggedFibonacciGenerator* lfg, u32 seed_out[SEED_SIZE])
{
  if (size < LFG_K)
    return false;

  // If the data doesn't look like something we can regenerate, return early to save time
  if (!std::all_of(data, data + LFG_K, [](u32 x) {
        return (Common::swap32(x) & 0x00C00000) == (Common::swap32(x) >> 2 & 0x00C00000);
      }))
  {
    return false;
  }

  const size_t data_offset_mod_k = data_offset % LFG_K;
  const size_t data_offset_div_k = data_offset / LFG_K;

  std::copy(data, data + LFG_K - data_offset_mod_k, lfg->m_buffer.data() + data_offset_mod_k);
  std::copy(data + LFG_K - data_offset_mod_k, data + LFG_K, lfg->m_buffer.data());

  lfg->Backward(0, data_offset_mod_k);

  for (size_t i = 0; i < data_offset_div_k; ++i)
    lfg->Backward();

  if (!lfg->Reinitialize(seed_out))
    return false;

  for (size_t i = 0; i < data_offset_div_k; ++i)
    lfg->Forward();

  return true;
}

void LaggedFibonacciGenerator::GetBytes(size_t count, u8* out)
{
  while (count > 0)
  {
    const size_t length = std::min(count, LFG_K * sizeof(u32) - m_position_bytes);

    std::memcpy(out, reinterpret_cast<u8*>(m_buffer.data()) + m_position_bytes, length);

    m_position_bytes += length;
    count -= length;
    out += length;

    if (m_position_bytes == LFG_K * sizeof(u32))
    {
      Forward();
      m_position_bytes = 0;
    }
  }
}

u8 LaggedFibonacciGenerator::GetByte()
{
  const u8 result = reinterpret_cast<u8*>(m_buffer.data())[m_position_bytes];

  ++m_position_bytes;

  if (m_position_bytes == LFG_K * sizeof(u32))
  {
    Forward();
    m_position_bytes = 0;
  }

  return result;
}

void LaggedFibonacciGenerator::Forward(size_t count)
{
  m_position_bytes += count;
  while (m_position_bytes >= LFG_K * sizeof(u32))
  {
    Forward();
    m_position_bytes -= LFG_K * sizeof(u32);
  }
}

void LaggedFibonacciGenerator::Forward()
{
  for (size_t i = 0; i < LFG_J; ++i)
    m_buffer[i] ^= m_buffer[i + LFG_K - LFG_J];

  for (size_t i = LFG_J; i < LFG_K; ++i)
    m_buffer[i] ^= m_buffer[i - LFG_J];
}

void LaggedFibonacciGenerator::Backward(size_t start_word, size_t end_word)
{
  const size_t loop_end = std::max(LFG_J, start_word);
  for (size_t i = std::min(end_word, LFG_K); i > loop_end; --i)
    m_buffer[i - 1] ^= m_buffer[i - 1 - LFG_J];

  for (size_t i = std::min(end_word, LFG_J); i > start_word; --i)
    m_buffer[i - 1] ^= m_buffer[i - 1 + LFG_K - LFG_J];
}

bool LaggedFibonacciGenerator::Reinitialize(u32 seed_out[SEED_SIZE])
{
  for (size_t i = 0; i < 4; ++i)
    Backward();

  for (u32& x : m_buffer)
    x = Common::swap32(x);

  // Reconstruct the bits which are missing due to the output code shifting by 18 instead of 16.
  // Unfortunately we can't reconstruct bits 16 and 17 (counting LSB as 0) for the first word,
  // but the observable result (when shifting by 18 instead of 16) is not affected by this.
  for (size_t i = 0; i < SEED_SIZE; ++i)
  {
    m_buffer[i] = (m_buffer[i] & 0xFF00FFFF) | (m_buffer[i] << 2 & 0x00FC0000) |
                  ((m_buffer[i + 16] ^ m_buffer[i + 15]) << 9 & 0x00030000);
  }

  for (size_t i = 0; i < SEED_SIZE; ++i)
    seed_out[i] = Common::swap32(m_buffer[i]);

  return Initialize(true);
}

bool LaggedFibonacciGenerator::Initialize(bool check_existing_data)
{
  for (size_t i = SEED_SIZE; i < LFG_K; ++i)
  {
    const u32 calculated = (m_buffer[i - 17] << 23) ^ (m_buffer[i - 16] >> 9) ^ m_buffer[i - 1];

    if (check_existing_data)
    {
      const u32 actual = (m_buffer[i] & 0xFF00FFFF) | (m_buffer[i] << 2 & 0x00FC0000);
      if ((calculated & 0xFFFCFFFF) != actual)
        return false;
    }

    m_buffer[i] = calculated;
  }

  // Instead of doing the "shift by 18 instead of 16" oddity when actually outputting the data,
  // we can do the shifting (and byteswapping) at this point to make the output code simpler.
  for (u32& x : m_buffer)
    x = Common::swap32((x & 0xFF00FFFF) | ((x >> 2) & 0x00FF0000));

  for (size_t i = 0; i < 4; ++i)
    Forward();

  return true;
}

}  // namespace DiscIO
