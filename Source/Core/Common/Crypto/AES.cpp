// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <bit>
#include <memory>

#include <mbedtls/aes.h>

#include "Common/Assert.h"
#include "Common/CPUDetect.h"
#include "Common/Crypto/AES.h"

#ifdef _MSC_VER
#include <intrin.h>
#else
#if defined(_M_X86_64)
#include <x86intrin.h>
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

namespace Common::AES
{
// For x64 and arm64, it's very unlikely a user's cpu does not support the accelerated version,
// fallback is just in case.
template <Mode AesMode>
class ContextGeneric final : public Context
{
public:
  ContextGeneric(const u8* key)
  {
    mbedtls_aes_init(&ctx);
    if constexpr (AesMode == Mode::Encrypt)
      ASSERT(!mbedtls_aes_setkey_enc(&ctx, key, 128));
    else
      ASSERT(!mbedtls_aes_setkey_dec(&ctx, key, 128));
  }

  virtual bool Crypt(const u8* iv, u8* iv_out, const u8* buf_in, u8* buf_out,
                     size_t len) const override
  {
    std::array<u8, BLOCK_SIZE> iv_tmp{};
    if (iv)
      std::memcpy(&iv_tmp[0], iv, BLOCK_SIZE);

    constexpr int mode = (AesMode == Mode::Encrypt) ? MBEDTLS_AES_ENCRYPT : MBEDTLS_AES_DECRYPT;
    if (mbedtls_aes_crypt_cbc(const_cast<mbedtls_aes_context*>(&ctx), mode, len, &iv_tmp[0], buf_in,
                              buf_out))
      return false;

    if (iv_out)
      std::memcpy(iv_out, &iv_tmp[0], BLOCK_SIZE);
    return true;
  }

private:
  mbedtls_aes_context ctx{};
};

#if defined(_M_X86_64)

// Note that (for instructions with same data width) the actual instructions emitted vary depending
// on compiler and flags. The naming is somewhat confusing, because VAES cpuid flag was added after
// VAES(VEX.128):
// clang-format off
// instructions   | cpuid flag      | #define
// AES(128)       | AES             | -
// VAES(VEX.128)  | AES & AVX       | __AVX__
// VAES(VEX.256)  | VAES            | -
// VAES(EVEX.128) | VAES & AVX512VL | __AVX512VL__
// VAES(EVEX.256) | VAES & AVX512VL | __AVX512VL__
// VAES(EVEX.512) | VAES & AVX512F  | __AVX512F__
// clang-format on
template <Mode AesMode>
class ContextAESNI final : public Context
{
  static inline __m128i Aes128KeygenAssistFinish(__m128i key, __m128i kga)
  {
    __m128i tmp = _mm_shuffle_epi32(kga, _MM_SHUFFLE(3, 3, 3, 3));
    tmp = _mm_xor_si128(tmp, key);

    key = _mm_slli_si128(key, 4);
    tmp = _mm_xor_si128(tmp, key);
    key = _mm_slli_si128(key, 4);
    tmp = _mm_xor_si128(tmp, key);
    key = _mm_slli_si128(key, 4);
    tmp = _mm_xor_si128(tmp, key);
    return tmp;
  }

  template <size_t RoundIdx>
  ATTRIBUTE_TARGET("aes")
  inline constexpr void StoreRoundKey(__m128i rk)
  {
    if constexpr (AesMode == Mode::Encrypt)
      round_keys[RoundIdx] = rk;
    else
    {
      constexpr size_t idx = NUM_ROUND_KEYS - RoundIdx - 1;
      if constexpr (idx == 0 || idx == NUM_ROUND_KEYS - 1)
        round_keys[idx] = rk;
      else
        round_keys[idx] = _mm_aesimc_si128(rk);
    }
  }

  template <size_t RoundIdx, int Rcon>
  ATTRIBUTE_TARGET("aes")
  inline constexpr __m128i Aes128Keygen(__m128i rk)
  {
    rk = Aes128KeygenAssistFinish(rk, _mm_aeskeygenassist_si128(rk, Rcon));
    StoreRoundKey<RoundIdx>(rk);
    return rk;
  }

public:
  ContextAESNI(const u8* key)
  {
    __m128i rk = _mm_loadu_si128((const __m128i*)key);
    StoreRoundKey<0>(rk);
    rk = Aes128Keygen<1, 0x01>(rk);
    rk = Aes128Keygen<2, 0x02>(rk);
    rk = Aes128Keygen<3, 0x04>(rk);
    rk = Aes128Keygen<4, 0x08>(rk);
    rk = Aes128Keygen<5, 0x10>(rk);
    rk = Aes128Keygen<6, 0x20>(rk);
    rk = Aes128Keygen<7, 0x40>(rk);
    rk = Aes128Keygen<8, 0x80>(rk);
    rk = Aes128Keygen<9, 0x1b>(rk);
    Aes128Keygen<10, 0x36>(rk);
  }

  ATTRIBUTE_TARGET("aes")
  inline void CryptBlock(__m128i* iv, const u8* buf_in, u8* buf_out) const
  {
    __m128i block = _mm_loadu_si128((const __m128i*)buf_in);

    if constexpr (AesMode == Mode::Encrypt)
    {
      block = _mm_xor_si128(_mm_xor_si128(block, *iv), round_keys[0]);

      for (size_t i = 1; i < Nr; ++i)
        block = _mm_aesenc_si128(block, round_keys[i]);
      block = _mm_aesenclast_si128(block, round_keys[Nr]);

      *iv = block;
    }
    else
    {
      __m128i iv_next = block;

      block = _mm_xor_si128(block, round_keys[0]);

      for (size_t i = 1; i < Nr; ++i)
        block = _mm_aesdec_si128(block, round_keys[i]);
      block = _mm_aesdeclast_si128(block, round_keys[Nr]);

      block = _mm_xor_si128(block, *iv);
      *iv = iv_next;
    }

    _mm_storeu_si128((__m128i*)buf_out, block);
  }

  // Takes advantage of instruction pipelining to parallelize.
  template <size_t NumBlocks>
  ATTRIBUTE_TARGET("aes")
  inline void DecryptPipelined(__m128i* iv, const u8* buf_in, u8* buf_out) const
  {
    constexpr size_t Depth = NumBlocks;

    __m128i block[Depth];
    for (size_t d = 0; d < Depth; d++)
      block[d] = _mm_loadu_si128(&((const __m128i*)buf_in)[d]);

    __m128i iv_next[1 + Depth];
    iv_next[0] = *iv;
    for (size_t d = 0; d < Depth; d++)
      iv_next[1 + d] = block[d];

    for (size_t d = 0; d < Depth; d++)
      block[d] = _mm_xor_si128(block[d], round_keys[0]);

    // The main speedup is here
    for (size_t i = 1; i < Nr; ++i)
      for (size_t d = 0; d < Depth; d++)
        block[d] = _mm_aesdec_si128(block[d], round_keys[i]);
    for (size_t d = 0; d < Depth; d++)
      block[d] = _mm_aesdeclast_si128(block[d], round_keys[Nr]);

    for (size_t d = 0; d < Depth; d++)
      block[d] = _mm_xor_si128(block[d], iv_next[d]);
    *iv = iv_next[1 + Depth - 1];

    for (size_t d = 0; d < Depth; d++)
      _mm_storeu_si128(&((__m128i*)buf_out)[d], block[d]);
  }

  virtual bool Crypt(const u8* iv, u8* iv_out, const u8* buf_in, u8* buf_out,
                     size_t len) const override
  {
    if (len % BLOCK_SIZE)
      return false;

    __m128i iv_block = iv ? _mm_loadu_si128((const __m128i*)iv) : _mm_setzero_si128();

    if constexpr (AesMode == Mode::Decrypt)
    {
      // On amd zen2...(benchmark, not real-world):
      // With AES(128) instructions, BLOCK_DEPTH results in following speedup vs. non-pipelined: 4:
      // 18%, 8: 22%, 9: 26%, 10-15: 31%. 16: 8% (register exhaustion). With VAES(VEX.128), 10 gives
      // 36% speedup vs. its corresponding baseline. VAES(VEX.128) is ~4% faster than AES(128). The
      // result is similar on zen3.
      // Zen3 in general is 20% faster than zen2 in aes, and VAES(VEX.256) is 35% faster than
      // zen3/VAES(VEX.128).
      //  It seems like VAES(VEX.256) should be faster?
      // TODO Choose value at runtime based on some criteria?
      constexpr size_t BLOCK_DEPTH = 10;
      constexpr size_t CHUNK_LEN = BLOCK_DEPTH * BLOCK_SIZE;
      while (len >= CHUNK_LEN)
      {
        DecryptPipelined<BLOCK_DEPTH>(&iv_block, buf_in, buf_out);
        buf_in += CHUNK_LEN;
        buf_out += CHUNK_LEN;
        len -= CHUNK_LEN;
      }
    }

    len /= BLOCK_SIZE;
    while (len--)
    {
      CryptBlock(&iv_block, buf_in, buf_out);
      buf_in += BLOCK_SIZE;
      buf_out += BLOCK_SIZE;
    }

    if (iv_out)
      _mm_storeu_si128((__m128i*)iv_out, iv_block);

    return true;
  }

private:
  // Ensures alignment specifiers are respected.
  struct XmmReg
  {
    __m128i data;

    XmmReg& operator=(const __m128i& m)
    {
      data = m;
      return *this;
    }
    operator __m128i() const { return data; }
  };
  std::array<XmmReg, NUM_ROUND_KEYS> round_keys;
};

#endif

#if defined(_M_ARM_64)

template <Mode AesMode>
class ContextNeon final : public Context
{
public:
  template <size_t RoundIdx>
  inline constexpr void StoreRoundKey(const u32* rk)
  {
    const uint8x16_t rk_block = vreinterpretq_u8_u32(vld1q_u32(rk));
    if constexpr (AesMode == Mode::Encrypt)
      round_keys[RoundIdx] = rk_block;
    else
    {
      constexpr size_t idx = NUM_ROUND_KEYS - RoundIdx - 1;
      if constexpr (idx == 0 || idx == NUM_ROUND_KEYS - 1)
        round_keys[idx] = rk_block;
      else
        round_keys[idx] = vaesimcq_u8(rk_block);
    }
  }

  ContextNeon(const u8* key)
  {
    constexpr u8 rcon[]{0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36};
    std::array<u32, Nb * NUM_ROUND_KEYS> rk{};

    // This uses a nice trick I've seen in wolfssl (not sure original author),
    // which uses vaeseq_u8 to assist keygen.
    // vaeseq_u8: op1 = SubBytes(ShiftRows(AddRoundKey(op1, op2)))
    // given RotWord == ShiftRows for row 1 (rol(x,8))
    // Probably not super fast (moves to/from vector regs constantly), but it is nice and simple.

    std::memcpy(&rk[0], key, KEY_SIZE);
    StoreRoundKey<0>(&rk[0]);
    for (size_t i = 0; i < rk.size() - Nk; i += Nk)
    {
      const uint8x16_t enc = vaeseq_u8(vreinterpretq_u8_u32(vmovq_n_u32(rk[i + 3])), vmovq_n_u8(0));
      const u32 temp = vgetq_lane_u32(vreinterpretq_u32_u8(enc), 0);
      rk[i + 4] = rk[i + 0] ^ std::rotr(temp, 8) ^ rcon[i / Nk];
      rk[i + 5] = rk[i + 4] ^ rk[i + 1];
      rk[i + 6] = rk[i + 5] ^ rk[i + 2];
      rk[i + 7] = rk[i + 6] ^ rk[i + 3];
      // clang-format off
      // Not great
      const size_t rki = 1 + i / Nk;
      switch (rki)
      {
        case  1: StoreRoundKey< 1>(&rk[Nk * rki]); break;
        case  2: StoreRoundKey< 2>(&rk[Nk * rki]); break;
        case  3: StoreRoundKey< 3>(&rk[Nk * rki]); break;
        case  4: StoreRoundKey< 4>(&rk[Nk * rki]); break;
        case  5: StoreRoundKey< 5>(&rk[Nk * rki]); break;
        case  6: StoreRoundKey< 6>(&rk[Nk * rki]); break;
        case  7: StoreRoundKey< 7>(&rk[Nk * rki]); break;
        case  8: StoreRoundKey< 8>(&rk[Nk * rki]); break;
        case  9: StoreRoundKey< 9>(&rk[Nk * rki]); break;
        case 10: StoreRoundKey<10>(&rk[Nk * rki]); break;
      }
      // clang-format on
    }
  }

  inline void CryptBlock(uint8x16_t* iv, const u8* buf_in, u8* buf_out) const
  {
    uint8x16_t block = vld1q_u8(buf_in);

    if constexpr (AesMode == Mode::Encrypt)
    {
      block = veorq_u8(block, *iv);

      for (size_t i = 0; i < Nr - 1; ++i)
        block = vaesmcq_u8(vaeseq_u8(block, round_keys[i]));
      block = vaeseq_u8(block, round_keys[Nr - 1]);
      block = veorq_u8(block, round_keys[Nr]);

      *iv = block;
    }
    else
    {
      uint8x16_t iv_next = block;

      for (size_t i = 0; i < Nr - 1; ++i)
        block = vaesimcq_u8(vaesdq_u8(block, round_keys[i]));
      block = vaesdq_u8(block, round_keys[Nr - 1]);
      block = veorq_u8(block, round_keys[Nr]);

      block = veorq_u8(block, *iv);
      *iv = iv_next;
    }

    vst1q_u8(buf_out, block);
  }

  virtual bool Crypt(const u8* iv, u8* iv_out, const u8* buf_in, u8* buf_out,
                     size_t len) const override
  {
    if (len % BLOCK_SIZE)
      return false;

    uint8x16_t iv_block = iv ? vld1q_u8(iv) : vmovq_n_u8(0);

    len /= BLOCK_SIZE;
    while (len--)
    {
      CryptBlock(&iv_block, buf_in, buf_out);
      buf_in += BLOCK_SIZE;
      buf_out += BLOCK_SIZE;
    }

    if (iv_out)
      vst1q_u8(iv_out, iv_block);

    return true;
  }

private:
  std::array<uint8x16_t, NUM_ROUND_KEYS> round_keys;
};

#endif

template <Mode AesMode>
std::unique_ptr<Context> CreateContext(const u8* key)
{
  if (cpu_info.bAES)
  {
#if defined(_M_X86_64)
#if defined(__AVX__)
    // If compiler enables AVX, the intrinsics will generate VAES(VEX.128) instructions.
    // In the future we may want to compile the code twice and explicitly override the compiler
    // flags. There doesn't seem to be much performance difference between AES(128) and
    // VAES(VEX.128) at the moment, though.
    if (cpu_info.bAVX)
#endif
      return std::make_unique<ContextAESNI<AesMode>>(key);
#elif defined(_M_ARM_64)
    return std::make_unique<ContextNeon<AesMode>>(key);
#endif
  }
  return std::make_unique<ContextGeneric<AesMode>>(key);
}

std::unique_ptr<Context> CreateContextEncrypt(const u8* key)
{
  return CreateContext<Mode::Encrypt>(key);
}

std::unique_ptr<Context> CreateContextDecrypt(const u8* key)
{
  return CreateContext<Mode::Decrypt>(key);
}

// OFB encryption and decryption are the exact same. We don't encrypt though.
void CryptOFB(const u8* key, const u8* iv, u8* iv_out, const u8* buf_in, u8* buf_out, size_t size)
{
  mbedtls_aes_context aes_ctx;
  size_t iv_offset = 0;

  std::array<u8, 16> iv_tmp{};
  if (iv)
    std::memcpy(&iv_tmp[0], iv, 16);

  ASSERT(!mbedtls_aes_setkey_enc(&aes_ctx, key, 128));
  mbedtls_aes_crypt_ofb(&aes_ctx, size, &iv_offset, &iv_tmp[0], buf_in, buf_out);

  if (iv_out)
    std::memcpy(iv_out, &iv_tmp[0], 16);
}

}  // namespace Common::AES
