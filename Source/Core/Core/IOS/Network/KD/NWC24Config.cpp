// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/Network/KD/NWC24Config.h"

#include <cstring>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"
#include "Core/IOS/FS/FileSystem.h"
#include "Core/IOS/Uids.h"

namespace IOS::HLE::NWC24
{
constexpr const char CONFIG_PATH[] = "/" WII_WC24CONF_DIR "/nwc24msg.cfg";
constexpr const char CBK_PATH[] = "/" WII_WC24CONF_DIR "/nwc24msg.cbk";

NWC24Config::NWC24Config(std::shared_ptr<FS::FileSystem> fs) : m_fs{std::move(fs)}
{
  ReadConfig();
}

void NWC24Config::ReadConfig()
{
  if (const auto file = m_fs->OpenFile(PID_KD, PID_KD, CONFIG_PATH, FS::Mode::Read))
  {
    if (file->Read(&m_data, 1))
    {
      const s32 config_error = CheckNwc24Config();
      if (config_error)
        ERROR_LOG_FMT(IOS_WC24, "There is an error in the config for for WC24: {}", config_error);

      return;
    }
  }
  ResetConfig();
}

void NWC24Config::WriteCBK() const
{
  WriteConfigToPath(CBK_PATH);
}

void NWC24Config::WriteConfig() const
{
  WriteConfigToPath(CONFIG_PATH);
}

void NWC24Config::WriteConfigToPath(const std::string& path) const
{
  constexpr FS::Modes public_modes{FS::Mode::ReadWrite, FS::Mode::ReadWrite, FS::Mode::ReadWrite};
  m_fs->CreateFullPath(PID_KD, PID_KD, path, 0, public_modes);
  const auto file = m_fs->CreateAndOpenFile(PID_KD, PID_KD, path, public_modes);
  if (!file || !file->Write(&m_data, 1))
    ERROR_LOG_FMT(IOS_WC24, "Failed to open or write WC24 config file at {}", path);
}

void NWC24Config::ResetConfig()
{
  m_fs->Delete(PID_KD, PID_KD, CONFIG_PATH);

  constexpr const char* urls[5] = {
      "https://amw.wc24.wii.com/cgi-bin/account.cgi", "http://rcw.wc24.wii.com/cgi-bin/check.cgi",
      "http://mtw.wc24.wii.com/cgi-bin/receive.cgi",  "http://mtw.wc24.wii.com/cgi-bin/delete.cgi",
      "http://mtw.wc24.wii.com/cgi-bin/send.cgi",
  };

  memset(&m_data, 0, sizeof(m_data));

  SetMagic(0x57634366);
  SetVersion(8);
  SetCreationStage(NWC24CreationStage::Initial);
  SetEnableBooting(0);
  SetEmail("@wii.com");

  for (int i = 0; i < URL_COUNT; ++i)
  {
    strncpy(m_data.http_urls[i], urls[i], MAX_URL_LENGTH);
  }

  SetChecksum(CalculateNwc24ConfigChecksum());

  WriteConfig();
}

u32 NWC24Config::CalculateNwc24ConfigChecksum() const
{
  const u32* ptr = reinterpret_cast<const u32*>(&m_data);
  u32 sum = 0;

  for (int i = 0; i < 0xFF; ++i)
  {
    sum += Common::swap32(*ptr++);
  }

  return sum;
}

s32 NWC24Config::CheckNwc24Config() const
{
  // 'WcCf' magic
  if (Magic() != 0x57634366)
  {
    ERROR_LOG_FMT(IOS_WC24, "Magic mismatch");
    return -14;
  }

  const u32 checksum = CalculateNwc24ConfigChecksum();
  DEBUG_LOG_FMT(IOS_WC24, "Checksum: {:X}", checksum);
  if (Checksum() != checksum)
  {
    ERROR_LOG_FMT(IOS_WC24, "Checksum mismatch expected {:X} and got {:X}", checksum, Checksum());
    return -14;
  }

  if (IdGen() > 0x1F)
  {
    ERROR_LOG_FMT(IOS_WC24, "Id gen error");
    return -14;
  }

  if (Version() != 8)
    return -27;

  return 0;
}

u32 NWC24Config::Magic() const
{
  return Common::swap32(m_data.magic);
}

void NWC24Config::SetMagic(u32 magic)
{
  m_data.magic = Common::swap32(magic);
}

u32 NWC24Config::Version() const
{
  return Common::swap32(m_data.version);
}

void NWC24Config::SetVersion(u32 version)
{
  m_data.version = Common::swap32(version);
}

u32 NWC24Config::IdGen() const
{
  return Common::swap32(m_data.id_generation);
}

void NWC24Config::SetIdGen(u32 id_generation)
{
  m_data.id_generation = Common::swap32(id_generation);
}

void NWC24Config::IncrementIdGen()
{
  u32 id_ctr = IdGen();
  id_ctr++;
  id_ctr &= 0x1F;

  SetIdGen(id_ctr);
}

u32 NWC24Config::Checksum() const
{
  return Common::swap32(m_data.checksum);
}

void NWC24Config::SetChecksum(u32 checksum)
{
  m_data.checksum = Common::swap32(checksum);
}

NWC24CreationStage NWC24Config::CreationStage() const
{
  return NWC24CreationStage(Common::swap32(u32(m_data.creation_stage)));
}

void NWC24Config::SetCreationStage(NWC24CreationStage creation_stage)
{
  m_data.creation_stage = NWC24CreationStage(Common::swap32(u32(creation_stage)));
}

u32 NWC24Config::EnableBooting() const
{
  return Common::swap32(m_data.enable_booting);
}

void NWC24Config::SetEnableBooting(u32 enable_booting)
{
  m_data.enable_booting = Common::swap32(enable_booting);
}

u64 NWC24Config::Id() const
{
  return Common::swap64(m_data.nwc24_id);
}

void NWC24Config::SetId(u64 nwc24_id)
{
  m_data.nwc24_id = Common::swap64(nwc24_id);
}

const char* NWC24Config::Email() const
{
  return m_data.email;
}

void NWC24Config::SetEmail(const char* email)
{
  strncpy(m_data.email, email, MAX_EMAIL_LENGTH);
  m_data.email[MAX_EMAIL_LENGTH - 1] = '\0';
}

std::string_view NWC24Config::GetMlchkid() const
{
  const size_t size = strnlen(m_data.mlchkid, MAX_MLCHKID_LENGTH);
  return {m_data.mlchkid, size};
}

std::string NWC24Config::GetCheckURL() const
{
  const size_t size = strnlen(m_data.http_urls[1], MAX_URL_LENGTH);
  return {m_data.http_urls[1], size};
}

std::string NWC24Config::GetAccountURL() const
{
  const size_t size = strnlen(m_data.http_urls[0], MAX_URL_LENGTH);
  return {m_data.http_urls[0], size};
}

void NWC24Config::SetMailCheckID(std::string_view mlchkid)
{
  std::strncpy(m_data.mlchkid, mlchkid.data(), std::size(m_data.mlchkid));
  m_data.mlchkid[MAX_MLCHKID_LENGTH - 1] = '\0';
}

void NWC24Config::SetPassword(std::string_view password)
{
  std::strncpy(m_data.paswd, password.data(), std::size(m_data.paswd));
  m_data.paswd[MAX_PASSWORD_LENGTH - 1] = '\0';
}

std::string NWC24Config::GetSendURL() const
{
  const size_t size = strnlen(m_data.http_urls[4], MAX_URL_LENGTH);
  return {m_data.http_urls[4], size};
}

std::string_view NWC24Config::GetPassword() const
{
  const size_t size = strnlen(m_data.paswd, MAX_PASSWORD_LENGTH);
  return {m_data.paswd, size};
}
}  // namespace IOS::HLE::NWC24
