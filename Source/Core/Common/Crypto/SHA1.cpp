// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "SHA1.h"

#include <array>
#include <memory>

#include <mbedtls/sha1.h>

#include "Common/Assert.h"
#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"
#include "Common/Swap.h"

#ifdef _MSC_VER
#include <intrin.h>
#else
#ifdef _M_X86_64
#include <immintrin.h>
#elif defined(_M_ARM_64)
#include <arm_acle.h>
#include <arm_neon.h>
#endif
#endif

#ifdef _MSC_VER
#define ATTRIBUTE_TARGET(x)
#else
#define ATTRIBUTE_TARGET(x) [[gnu::target(x)]]
#endif

namespace Common::SHA1
{
class ContextMbed final : public Context
{
public:
  ContextMbed()
  {
    mbedtls_sha1_init(&ctx);
    ASSERT(!mbedtls_sha1_starts_ret(&ctx));
  }
  ~ContextMbed() { mbedtls_sha1_free(&ctx); }
  virtual void Update(const u8* msg, size_t len) override
  {
    ASSERT(!mbedtls_sha1_update_ret(&ctx, msg, len));
  }
  virtual Digest Finish() override
  {
    Digest digest;
    ASSERT(!mbedtls_sha1_finish_ret(&ctx, digest.data()));
    return digest;
  }
  virtual bool HwAccelerated() const override { return false; }

private:
  mbedtls_sha1_context ctx{};
};

class BlockContext : public Context
{
protected:
  static constexpr size_t BLOCK_LEN = 64;
  static constexpr u32 K[4]{0x5a827999, 0x6ed9eba1, 0x8f1bbcdc, 0xca62c1d6};
  static constexpr u32 H[5]{0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0};

  virtual void ProcessBlock(const u8* msg) = 0;
  virtual Digest GetDigest() = 0;

  virtual void Update(const u8* msg, size_t len) override
  {
    if (len == 0)
      return;
    msg_len += len;

    if (block_used)
    {
      if (block_used + len >= block.size())
      {
        size_t rem = block.size() - block_used;
        std::memcpy(&block[block_used], msg, rem);
        ProcessBlock(&block[0]);
        block_used = 0;
        msg += rem;
        len -= rem;
      }
      else
      {
        std::memcpy(&block[block_used], msg, len);
        block_used += len;
        return;
      }
    }
    while (len >= BLOCK_LEN)
    {
      ProcessBlock(msg);
      msg += BLOCK_LEN;
      len -= BLOCK_LEN;
    }
    if (len)
    {
      std::memcpy(&block[0], msg, len);
      block_used = len;
    }
  }

  virtual Digest Finish() override
  {
    // block_used is guaranteed < BLOCK_LEN
    block[block_used++] = 0x80;

    constexpr size_t MSG_LEN_POS = BLOCK_LEN - sizeof(u64);
    if (block_used > MSG_LEN_POS)
    {
      // Pad current block and process it
      std::memset(&block[block_used], 0, BLOCK_LEN - block_used);
      ProcessBlock(&block[0]);

      // Pad a new block
      std::memset(&block[0], 0, MSG_LEN_POS);
    }
    else
    {
      // Pad current block
      std::memset(&block[block_used], 0, MSG_LEN_POS - block_used);
    }

    Common::BigEndianValue<u64> msg_bitlen(msg_len * 8);
    std::memcpy(&block[MSG_LEN_POS], &msg_bitlen, sizeof(msg_bitlen));

    ProcessBlock(&block[0]);

    return GetDigest();
  }

  alignas(64) std::array<u8, BLOCK_LEN> block{};
  size_t block_used{};
  size_t msg_len{};
};

template <typename ValueType, size_t Size>
class CyclicArray
{
public:
  inline ValueType operator[](size_t i) const { return data[i % Size]; }
  inline ValueType& operator[](size_t i) { return data[i % Size]; }
  constexpr size_t size() { return Size; }

private:
  std::array<ValueType, Size> data;
};

#ifdef _M_X86_64

// Uses the dedicated SHA1 instructions. Normal SSE(AVX*) would be needed for parallel
// multi-message processing. While Dolphin could gain from such implementation, it requires the
// calling code to be modified and/or making the SHA1 implementation asynchronous so it can
// optimistically batch.
class ContextX64SHA1 final : public BlockContext
{
public:
  ContextX64SHA1()
  {
    state[0] = _mm_set_epi32(H[0], H[1], H[2], H[3]);
    state[1] = _mm_set_epi32(H[4], 0, 0, 0);
  }

private:
  using WorkBlock = CyclicArray<__m128i, 4>;

  ATTRIBUTE_TARGET("ssse3")
  static inline __m128i byterev_16B(__m128i x)
  {
    return _mm_shuffle_epi8(x, _mm_set_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15));
  }

  template <size_t I>
  ATTRIBUTE_TARGET("sha")
  static inline __m128i MsgSchedule(WorkBlock* wblock)
  {
    auto& w = *wblock;
    // Update and return this location
    auto& wx = w[I];
    // Do all the xors and rol(x,1) required for 4 rounds of msg schedule
    wx = _mm_sha1msg1_epu32(wx, w[I + 1]);
    wx = _mm_xor_si128(wx, w[I + 2]);
    wx = _mm_sha1msg2_epu32(wx, w[I + 3]);
    return wx;
  }

  ATTRIBUTE_TARGET("sha")
  virtual void ProcessBlock(const u8* msg) override
  {
    // There are 80 rounds with 4 bytes per round, giving 0x140 byte work space, but we can keep
    // active state in just 0x40 bytes.
    // see FIPS 180-4 6.1.3 Alternate Method for Computing a SHA-1 Message Digest
    WorkBlock w;
    auto msg_block = (const __m128i*)msg;
    for (size_t i = 0; i < w.size(); i++)
      w[i] = byterev_16B(_mm_loadu_si128(&msg_block[i]));

    // 0: abcd, 1: e
    auto abcde = state;

    // Not sure of a (non-ugly) way to have constant-evaluated for-loop, so just rely on inlining.
    // Problem is that sha1rnds4 requires imm8 arg, and first/last rounds have different behavior.

    // clang-format off
    // E0 += MSG0, special case of "nexte", can do normal add
    abcde[1] = _mm_sha1rnds4_epu32(abcde[0], _mm_add_epi32(abcde[1], w[0]), 0);
    abcde[0] = _mm_sha1rnds4_epu32(abcde[1], _mm_sha1nexte_epu32(abcde[0], w[1]), 0);
    abcde[1] = _mm_sha1rnds4_epu32(abcde[0], _mm_sha1nexte_epu32(abcde[1], w[2]), 0);
    abcde[0] = _mm_sha1rnds4_epu32(abcde[1], _mm_sha1nexte_epu32(abcde[0], w[3]), 0);
    abcde[1] = _mm_sha1rnds4_epu32(abcde[0], _mm_sha1nexte_epu32(abcde[1], MsgSchedule<4>(&w)), 0);
    abcde[0] = _mm_sha1rnds4_epu32(abcde[1], _mm_sha1nexte_epu32(abcde[0], MsgSchedule<5>(&w)), 1);
    abcde[1] = _mm_sha1rnds4_epu32(abcde[0], _mm_sha1nexte_epu32(abcde[1], MsgSchedule<6>(&w)), 1);
    abcde[0] = _mm_sha1rnds4_epu32(abcde[1], _mm_sha1nexte_epu32(abcde[0], MsgSchedule<7>(&w)), 1);
    abcde[1] = _mm_sha1rnds4_epu32(abcde[0], _mm_sha1nexte_epu32(abcde[1], MsgSchedule<8>(&w)), 1);
    abcde[0] = _mm_sha1rnds4_epu32(abcde[1], _mm_sha1nexte_epu32(abcde[0], MsgSchedule<9>(&w)), 1);
    abcde[1] = _mm_sha1rnds4_epu32(abcde[0], _mm_sha1nexte_epu32(abcde[1], MsgSchedule<10>(&w)), 2);
    abcde[0] = _mm_sha1rnds4_epu32(abcde[1], _mm_sha1nexte_epu32(abcde[0], MsgSchedule<11>(&w)), 2);
    abcde[1] = _mm_sha1rnds4_epu32(abcde[0], _mm_sha1nexte_epu32(abcde[1], MsgSchedule<12>(&w)), 2);
    abcde[0] = _mm_sha1rnds4_epu32(abcde[1], _mm_sha1nexte_epu32(abcde[0], MsgSchedule<13>(&w)), 2);
    abcde[1] = _mm_sha1rnds4_epu32(abcde[0], _mm_sha1nexte_epu32(abcde[1], MsgSchedule<14>(&w)), 2);
    abcde[0] = _mm_sha1rnds4_epu32(abcde[1], _mm_sha1nexte_epu32(abcde[0], MsgSchedule<15>(&w)), 3);
    abcde[1] = _mm_sha1rnds4_epu32(abcde[0], _mm_sha1nexte_epu32(abcde[1], MsgSchedule<16>(&w)), 3);
    abcde[0] = _mm_sha1rnds4_epu32(abcde[1], _mm_sha1nexte_epu32(abcde[0], MsgSchedule<17>(&w)), 3);
    abcde[1] = _mm_sha1rnds4_epu32(abcde[0], _mm_sha1nexte_epu32(abcde[1], MsgSchedule<18>(&w)), 3);
    abcde[0] = _mm_sha1rnds4_epu32(abcde[1], _mm_sha1nexte_epu32(abcde[0], MsgSchedule<19>(&w)), 3);
    // state += abcde
    state[1] = _mm_sha1nexte_epu32(abcde[1], state[1]);
    state[0] = _mm_add_epi32(abcde[0], state[0]);
    // clang-format on
  }

  virtual Digest GetDigest() override
  {
    Digest digest;
    _mm_storeu_si128((__m128i*)&digest[0], byterev_16B(state[0]));
    u32 hi = _mm_cvtsi128_si32(byterev_16B(state[1]));
    std::memcpy(&digest[sizeof(__m128i)], &hi, sizeof(hi));
    return digest;
  }

  virtual bool HwAccelerated() const override { return true; }

  std::array<__m128i, 2> state{};
};

#endif

#ifdef _M_ARM_64

class ContextNeon final : public BlockContext
{
public:
  ContextNeon()
  {
    state.abcd = vld1q_u32(&H[0]);
    state.e = H[4];
  }

private:
  using WorkBlock = CyclicArray<uint32x4_t, 4>;

  struct State
  {
    // ARM thought they were being clever by exposing e as u32, but it actually makes non-asm
    // implementations pretty annoying/makes compiler's life needlessly difficult.
    uint32x4_t abcd{};
    u32 e{};
  };

  static inline uint32x4_t MsgSchedule(WorkBlock* wblock, size_t i)
  {
    auto& w = *wblock;
    // Update and return this location
    auto& wx = w[0 + i];
    wx = vsha1su0q_u32(wx, w[1 + i], w[2 + i]);
    wx = vsha1su1q_u32(wx, w[3 + i]);
    return wx;
  }

  template <size_t Func>
  static inline constexpr uint32x4_t f(State state, uint32x4_t w)
  {
    const auto wk = vaddq_u32(w, vdupq_n_u32(K[Func]));
    if constexpr (Func == 0)
      return vsha1cq_u32(state.abcd, state.e, wk);
    if constexpr (Func == 1 || Func == 3)
      return vsha1pq_u32(state.abcd, state.e, wk);
    if constexpr (Func == 2)
      return vsha1mq_u32(state.abcd, state.e, wk);
  }

  template <size_t Func>
  static inline constexpr State FourRounds(State state, uint32x4_t w)
  {
    return {f<Func>(state, w), vsha1h_u32(vgetq_lane_u32(state.abcd, 0))};
  }

  virtual void ProcessBlock(const u8* msg) override
  {
    WorkBlock w;
    for (size_t i = 0; i < w.size(); i++)
      w[i] = vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(&msg[sizeof(uint32x4_t) * i])));

    std::array<State, 2> states{state};

    // Fashioned to look like x64 impl.
    // In each case the goal is to have compiler inline + unroll everything.
    states[1] = FourRounds<0>(states[0], w[0]);
    states[0] = FourRounds<0>(states[1], w[1]);
    states[1] = FourRounds<0>(states[0], w[2]);
    states[0] = FourRounds<0>(states[1], w[3]);
    states[1] = FourRounds<0>(states[0], MsgSchedule(&w, 4));
    states[0] = FourRounds<1>(states[1], MsgSchedule(&w, 5));
    states[1] = FourRounds<1>(states[0], MsgSchedule(&w, 6));
    states[0] = FourRounds<1>(states[1], MsgSchedule(&w, 7));
    states[1] = FourRounds<1>(states[0], MsgSchedule(&w, 8));
    states[0] = FourRounds<1>(states[1], MsgSchedule(&w, 9));
    states[1] = FourRounds<2>(states[0], MsgSchedule(&w, 10));
    states[0] = FourRounds<2>(states[1], MsgSchedule(&w, 11));
    states[1] = FourRounds<2>(states[0], MsgSchedule(&w, 12));
    states[0] = FourRounds<2>(states[1], MsgSchedule(&w, 13));
    states[1] = FourRounds<2>(states[0], MsgSchedule(&w, 14));
    states[0] = FourRounds<3>(states[1], MsgSchedule(&w, 15));
    states[1] = FourRounds<3>(states[0], MsgSchedule(&w, 16));
    states[0] = FourRounds<3>(states[1], MsgSchedule(&w, 17));
    states[1] = FourRounds<3>(states[0], MsgSchedule(&w, 18));
    states[0] = FourRounds<3>(states[1], MsgSchedule(&w, 19));

    state = {vaddq_u32(state.abcd, states[0].abcd), state.e + states[0].e};
  }

  virtual Digest GetDigest() override
  {
    Digest digest;
    vst1q_u8(&digest[0], vrev32q_u8(vreinterpretq_u8_u32(state.abcd)));
    u32 e = Common::FromBigEndian(state.e);
    std::memcpy(&digest[sizeof(state.abcd)], &e, sizeof(e));
    return digest;
  }

  virtual bool HwAccelerated() const override { return true; }

  State state;
};

#endif

std::unique_ptr<Context> CreateContext()
{
  if (cpu_info.bSHA1)
  {
#ifdef _M_X86_64
    // Note: As of mid 2022, > 99% of CPUs reporting to Steam survey have SSSE3, ~40% have SHA.
    // Seems unlikely we'll see any cpus supporting SHA but not SSSE3 (in the foreseeable future at
    // least).
    if (cpu_info.bSSSE3)
      return std::make_unique<ContextX64SHA1>();
#elif defined(_M_ARM_64)
    return std::make_unique<ContextNeon>();
#endif
  }
  return std::make_unique<ContextMbed>();
}

Digest CalculateDigest(const u8* msg, size_t len)
{
  auto ctx = CreateContext();
  ctx->Update(msg, len);
  return ctx->Finish();
}
}  // namespace Common::SHA1
