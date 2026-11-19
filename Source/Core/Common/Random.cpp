// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Random.h"

#include <algorithm>

#include <wolfssl/wolfcrypt/random.h>
#include <wolfssl/wolfcrypt/wc_port.h>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"

namespace Common::Random
{
class EntropySeededPRNG final
{
public:
  EntropySeededPRNG()
  {
    // wc_InitRng() locks a global mutex which, on Windows, is only initialized by wolfCrypt_Init()
    // The reference is never released, since thread-local RNGs may be used until exit.
    static const int init_ret = wolfCrypt_Init();
    ASSERT(init_ret == 0);

    const int ret = wc_InitRng(&m_rng);
    ASSERT(ret == 0);
  }

  ~EntropySeededPRNG() { wc_FreeRng(&m_rng); }

  void Generate(void* buffer, std::size_t size)
  {
    u8* out = static_cast<u8*>(buffer);
    while (size != 0)
    {
      const std::size_t block_size = std::min<std::size_t>(size, RNG_MAX_BLOCK_LEN);
      const int ret = wc_RNG_GenerateBlock(&m_rng, out, static_cast<word32>(block_size));
      ASSERT(ret == 0);
      out += block_size;
      size -= block_size;
    }
  }

private:
  WC_RNG m_rng;
};

static thread_local EntropySeededPRNG s_esprng;

void Generate(void* buffer, std::size_t size)
{
  s_esprng.Generate(buffer, size);
}
}  // namespace Common::Random
