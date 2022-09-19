// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

namespace IOS::HLE::NWC24
{
#pragma pack(push, 1)
struct WC24File final
{
  char magic[4];
  u32 version;
  u32 filler;
  u8 crypt_type;
  u8 padding[3];
  u8 reserved[32];
  u8 iv[16];
  u8 rsa_signature[256];
};

struct WC24PubkMod final
{
  u8 rsa_public[256];
  u8 rsa_reserved[256];
  u8 aes_key[16];
  u8 aes_reserved[16];
};
#pragma pack(pop)
}  // namespace IOS::HLE::NWC24
