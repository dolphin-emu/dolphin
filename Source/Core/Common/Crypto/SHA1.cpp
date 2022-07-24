// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "SHA1.h"

#include <memory>

#include <mbedtls/sha1.h>

#include "Common/Assert.h"
#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"

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

private:
  mbedtls_sha1_context ctx{};
};

std::unique_ptr<Context> CreateContext()
{
  return std::make_unique<ContextMbed>();
}

Digest CalculateDigest(const u8* msg, size_t len)
{
  auto ctx = CreateContext();
  ctx->Update(msg, len);
  return ctx->Finish();
}
}  // namespace Common::SHA1
