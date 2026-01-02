// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>
#include "Common/CommonTypes.h"

namespace IOS::HLE
{
namespace FS
{
class FileSystem;
}
namespace NWC24
{
enum ErrorCode : s32
{
  WC24_OK = 0,
  WC24_ERR_FATAL = -1,
  WC24_ERR_INVALID_VALUE = -3,
  WC24_ERR_NOT_SUPPORTED = -4,
  WC24_ERR_NULL = -5,
  WC24_ERR_NOT_FOUND = -13,
  WC24_ERR_BROKEN = -14,
  WC24_ERR_FILE_OPEN = -16,
  WC24_ERR_FILE_CLOSE = -17,
  WC24_ERR_FILE_READ = -18,
  WC24_ERR_FILE_WRITE = -19,
  WC24_ERR_FORMAT = -24,
  WC24_ERR_NETWORK = -31,
  WC24_ERR_SERVER = -32,
  WC24_ERR_ID_NOT_GENERATED = -34,
  WC24_ERR_ID_GENERATED = -35,
  WC24_ERR_ID_REGISTERED = -36,
  WC24_ERR_DISABLED = -39,
  WC24_ERR_ID_NOT_REGISTERED = -44,
  WC24_MSG_DAMAGED = -71,
  WC24_MSG_TOO_BIG = -72
};

enum class NWC24CreationStage : u32
{
  Initial = 0,
  Generated = 1,
  Registered = 2
};

class NWC24Config final
{
public:
  explicit NWC24Config(std::shared_ptr<FS::FileSystem> fs);

  void ReadConfig();
  void WriteCBK() const;
  void WriteConfig() const;
  void WriteConfigToPath(const std::string& path) const;
  void ResetConfig();

  u32 CalculateNwc24ConfigChecksum() const;
  s32 CheckNwc24Config() const;

  u32 Magic() const;
  void SetMagic(u32 magic);

  u32 Version() const;
  void SetVersion(u32 version);

  u32 IdGen() const;
  void SetIdGen(u32 id_generation);
  void IncrementIdGen();

  u32 Checksum() const;
  void SetChecksum(u32 checksum);

  std::string_view GetMlchkid() const;
  std::string GetCheckURL() const;
  std::string GetSendURL() const;
  std::string_view GetPassword() const;
  std::string GetDeleteURL() const;
  std::string GetReceiveURL() const;

  NWC24CreationStage CreationStage() const;
  void SetCreationStage(NWC24CreationStage creation_stage);

  bool IsCreated() const { return CreationStage() == NWC24CreationStage::Initial; }
  bool IsGenerated() const { return CreationStage() == NWC24CreationStage::Generated; }
  bool IsRegistered() const { return CreationStage() == NWC24CreationStage::Registered; }

  u32 EnableBooting() const;
  void SetEnableBooting(u32 enable_booting);

  u64 Id() const;
  void SetId(u64 nwc24_id);

  std::string_view GetEmail() const;
  void SetEmail(const char* email);

  std::string GetAccountURL() const;
  void SetMailCheckID(std::string_view mlchkid);
  void SetPassword(std::string_view password);

private:
  enum
  {
    URL_COUNT = 0x05,
    MAX_URL_LENGTH = 0x80,
    MAX_EMAIL_LENGTH = 0x40,
    MAX_PASSWORD_LENGTH = 0x20,
    MAX_MLCHKID_LENGTH = 0x24,
  };

#pragma pack(push, 1)
  struct ConfigData final
  {
    u32 magic;    // 'WcCf' 0x57634366
    u32 version;  // must be 8
    u64 nwc24_id;
    u32 id_generation;
    NWC24CreationStage creation_stage;
    char email[MAX_EMAIL_LENGTH];
    char paswd[MAX_PASSWORD_LENGTH];
    char mlchkid[MAX_MLCHKID_LENGTH];
    char http_urls[URL_COUNT][MAX_URL_LENGTH];
    u8 reserved[0xDC];
    u32 enable_booting;
    u32 checksum;
  };
#pragma pack(pop)

  std::shared_ptr<FS::FileSystem> m_fs;
  ConfigData m_data;
};
}  // namespace NWC24
}  // namespace IOS::HLE
