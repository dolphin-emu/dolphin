// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Random.h"

#include <mbedtls/entropy.h>
#include <mbedtls/hmac_drbg.h>

#include "Common/Assert.h"

namespace Common::Random
{
class CSPRNG final
{
public:
  CSPRNG()
  {
    mbedtls_entropy_init(&m_entropy);
    mbedtls_hmac_drbg_init(&m_context);
    const int ret = mbedtls_hmac_drbg_seed(&m_context, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                                           mbedtls_entropy_func, &m_entropy, nullptr, 0);
    ASSERT(ret == 0);
  }

  ~CSPRNG()
  {
    mbedtls_hmac_drbg_free(&m_context);
    mbedtls_entropy_free(&m_entropy);
  }

  void Generate(void* buffer, std::size_t size)
  {
    const int ret = mbedtls_hmac_drbg_random(&m_context, static_cast<u8*>(buffer), size);
    ASSERT(ret == 0);
  }

private:
  mbedtls_entropy_context m_entropy;
  mbedtls_hmac_drbg_context m_context;
};

static thread_local CSPRNG s_csprng;

void Generate(void* buffer, std::size_t size)
{
  s_csprng.Generate(buffer, size);
}
}  // namespace Common::Random
