// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "Common/CommonTypes.h"

// Dolphin only uses/implements AES-128-CBC.

namespace Common::AES
{
enum class Mode
{
  Decrypt,
  Encrypt,
};

class Context
{
protected:
  static constexpr size_t Nk = 4;
  static constexpr size_t Nb = 4;
  static constexpr size_t Nr = 10;
  static constexpr size_t WORD_SIZE = sizeof(u32);
  static constexpr size_t NUM_ROUND_KEYS = Nr + 1;

public:
  static constexpr size_t KEY_SIZE = Nk * WORD_SIZE;
  static constexpr size_t BLOCK_SIZE = Nb * WORD_SIZE;

  Context() = default;
  virtual ~Context() = default;
  virtual bool Crypt(const u8* iv, u8* iv_out, const u8* buf_in, u8* buf_out, size_t len) const = 0;
  bool Crypt(const u8* iv, const u8* buf_in, u8* buf_out, size_t len) const
  {
    return Crypt(iv, nullptr, buf_in, buf_out, len);
  }
  bool CryptIvZero(const u8* buf_in, u8* buf_out, size_t len) const
  {
    return Crypt(nullptr, nullptr, buf_in, buf_out, len);
  }
};

std::unique_ptr<Context> CreateContextEncrypt(const u8* key);
std::unique_ptr<Context> CreateContextDecrypt(const u8* key);

// OFB decryption for WiiConnect24
void CryptOFB(const u8* key, const u8* iv, u8* iv_out, const u8* buf_in, u8* buf_out, size_t size);

}  // namespace Common::AES
