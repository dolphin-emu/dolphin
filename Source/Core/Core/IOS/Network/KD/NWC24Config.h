// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "Common/CommonTypes.h"

namespace IOS
{
namespace HLE
{
namespace NWC24
{
enum ErrorCode : s32
{
  WC24_OK = 0,
  WC24_ERR_FATAL = -1,
  WC24_ERR_ID_NONEXISTANCE = -34,
  WC24_ERR_ID_GENERATED = -35,
  WC24_ERR_ID_REGISTERED = -36,
  WC24_ERR_ID_NOT_REGISTERED = -44,
};

class NWC24Config final
{
public:
  enum
  {
    NWC24_IDCS_INITIAL = 0,
    NWC24_IDCS_GENERATED = 1,
    NWC24_IDCS_REGISTERED = 2
  };

  enum
  {
    URL_COUNT = 0x05,
    MAX_URL_LENGTH = 0x80,
    MAX_EMAIL_LENGTH = 0x40,
    MAX_PASSWORD_LENGTH = 0x20,
  };

  NWC24Config();

  void ReadConfig();
  void WriteConfig() const;
  void ResetConfig();

  u32 CalculateNwc24ConfigChecksum() const;
  s32 CheckNwc24Config() const;

  u32 Magic() const;
  void SetMagic(u32 magic);

  u32 Unk() const;
  void SetUnk(u32 unk_04);

  u32 IdGen() const;
  void SetIdGen(u32 id_generation);
  void IncrementIdGen();

  u32 Checksum() const;
  void SetChecksum(u32 checksum);

  u32 CreationStage() const;
  void SetCreationStage(u32 creation_stage);

  u32 EnableBooting() const;
  void SetEnableBooting(u32 enable_booting);

  u64 Id() const;
  void SetId(u64 nwc24_id);

  const char* Email() const;
  void SetEmail(const char* email);

private:
#pragma pack(push, 1)
  struct ConfigData final
  {
    u32 magic;   // 'WcCf' 0x57634366
    u32 unk_04;  // must be 8
    u64 nwc24_id;
    u32 id_generation;
    u32 creation_stage;  // 0:not_generated; 1:generated; 2:registered
    char email[MAX_EMAIL_LENGTH];
    char paswd[MAX_PASSWORD_LENGTH];
    char mlchkid[0x24];
    char http_urls[URL_COUNT][MAX_URL_LENGTH];
    u8 reserved[0xDC];
    u32 enable_booting;
    u32 checksum;
  };
#pragma pack(pop)

  std::string m_path;
  ConfigData m_data;
};
}  // namespace NWC24
}  // namespace HLE
}  // namespace IOS
