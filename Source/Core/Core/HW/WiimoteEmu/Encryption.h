// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
//
// Copyright (C) Hector Martin "marcan" (hector@marcansoft.com)

#pragma once

#include "Common/CommonTypes.h"

namespace WiimoteEmu
{
class EncryptionKey
{
public:
  void Generate(const u8* keydata);

  void Encrypt(u8* data, int addr, u8 len) const;
  void Decrypt(u8* data, int addr, u8 len) const;

private:
  u8 ft[8] = {};
  u8 sb[8] = {};
};

}  // namespace WiimoteEmu
