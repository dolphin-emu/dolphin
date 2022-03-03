// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Copyright (C) Hector Martin "marcan" (hector@marcansoft.com)

#pragma once

#include <array>

#include "Common/CommonTypes.h"

namespace WiimoteEmu
{
using SBox = std::array<u8, 256>;

class EncryptionKey
{
public:
  void Encrypt(u8* data, u32 addr, u32 len) const;
  void Decrypt(u8* data, u32 addr, u32 len) const;

  using RandData = std::array<u8, 10>;
  using KeyData = std::array<u8, 6>;

  void GenerateTables(const RandData& rand, const KeyData& key, const SBox& sbox_a,
                      const SBox& sbox_b);

  std::array<u8, 8> ft = {};
  std::array<u8, 8> sb = {};
};

class KeyGen
{
public:
  virtual ~KeyGen() = default;

  using ExtKeyData = std::array<u8, 16>;

  EncryptionKey GenerateFromExtensionKeyData(const ExtKeyData& key_data) const;

protected:
  virtual EncryptionKey::KeyData GenerateKeyData(const EncryptionKey::RandData& rand,
                                                 u32 idx) const = 0;
  virtual EncryptionKey GenerateTables(const EncryptionKey::RandData& rand,
                                       const EncryptionKey::KeyData& key, u32 idx) const = 0;
  virtual EncryptionKey GenerateFallbackTables(const EncryptionKey::RandData& rand,
                                               const EncryptionKey::KeyData& key) const = 0;
};

class KeyGen1stParty final : public KeyGen
{
private:
  EncryptionKey::KeyData GenerateKeyData(const EncryptionKey::RandData& rand,
                                         u32 idx) const final override;
  EncryptionKey GenerateTables(const EncryptionKey::RandData& rand,
                               const EncryptionKey::KeyData& key, u32 idx) const final override;
  EncryptionKey GenerateFallbackTables(const EncryptionKey::RandData& rand,
                                       const EncryptionKey::KeyData& key) const final override;
};

class KeyGen3rdParty final : public KeyGen
{
private:
  EncryptionKey::KeyData GenerateKeyData(const EncryptionKey::RandData& rand,
                                         u32 idx) const final override;
  EncryptionKey GenerateTables(const EncryptionKey::RandData& rand,
                               const EncryptionKey::KeyData& key, u32 idx) const final override;
  EncryptionKey GenerateFallbackTables(const EncryptionKey::RandData& rand,
                                       const EncryptionKey::KeyData& key) const final override;
};

}  // namespace WiimoteEmu
