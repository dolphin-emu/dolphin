// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <limits>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Common/Common.h"
#include "Common/CommonTypes.h"

namespace Common
{
class IniFile;
}

namespace Core
{
class CPUThreadGuard;
}

namespace DiscIO
{
enum class Language;
enum class Platform;
enum class Region;
struct Partition;
class Volume;
}  // namespace DiscIO

namespace IOS::ES
{
class TMDReader;
}  // namespace IOS::ES

struct BootParameters;

struct SConfig
{
  // Settings
  bool bAutomaticStart = false;
  bool bBootToPause = false;

  bool bJITNoBlockCache = false;
  bool bJITNoBlockLinking = false;

  bool bCopyWiiSaveNetplay = true;

  bool bWii = false;
  bool m_is_mios = false;

  DiscIO::Region m_region;

  // files
  std::string m_strBootROM;
  std::string m_strSRAM;

  std::string m_debugger_game_id;
  // TODO: remove this as soon as the ticket view hack in IOS/ES/Views is dropped.
  bool m_disc_booted_from_game_list = false;

  const std::string& GetGameID() const { return m_game_id; }
  const std::string& GetGameTDBID() const { return m_gametdb_id; }
  const std::string& GetTitleName() const { return m_title_name; }
  const std::string& GetTitleDescription() const { return m_title_description; }
  u64 GetTitleID() const { return m_title_id; }
  u16 GetRevision() const { return m_revision; }
  void ResetRunningGameMetadata();
  void SetRunningGameMetadata(const DiscIO::Volume& volume, const DiscIO::Partition& partition);
  void SetRunningGameMetadata(const IOS::ES::TMDReader& tmd, DiscIO::Platform platform);
  void SetRunningGameMetadata(const std::string& game_id);
  // Reloads title-specific map files, patches, custom textures, etc.
  // This should only be called after the new title has been loaded into memory.
  static void OnNewTitleLoad(const Core::CPUThreadGuard& guard);

  void LoadDefaults();
  static std::string MakeGameID(std::string_view file_name);
  bool SetPathsAndGameMetadata(const BootParameters& boot);
  DiscIO::Language GetCurrentLanguage(bool wii) const;
  DiscIO::Language GetLanguageAdjustedForRegion(bool wii, DiscIO::Region region) const;
  std::string GetGameTDBImageRegionCode(bool wii, DiscIO::Region region) const;

  Common::IniFile LoadDefaultGameIni() const;
  Common::IniFile LoadLocalGameIni() const;
  Common::IniFile LoadGameIni() const;

  static Common::IniFile LoadDefaultGameIni(const std::string& id, std::optional<u16> revision);
  static Common::IniFile LoadLocalGameIni(const std::string& id, std::optional<u16> revision);
  static Common::IniFile LoadGameIni(const std::string& id, std::optional<u16> revision);

  SConfig(const SConfig&) = delete;
  SConfig& operator=(const SConfig&) = delete;
  SConfig(SConfig&&) = delete;
  SConfig& operator=(SConfig&&) = delete;

  // Save settings
  void SaveSettings();

  // Load settings
  void LoadSettings();

  // Return the permanent and somewhat globally used instance of this struct
  static SConfig& GetInstance() { return (*m_Instance); }
  static void Init();
  static void Shutdown();

private:
  SConfig();
  ~SConfig();

  void SetRunningGameMetadata(const std::string& game_id, const std::string& gametdb_id,
                              u64 title_id, u16 revision, DiscIO::Region region);

  static SConfig* m_Instance;

  std::string m_game_id;
  std::string m_gametdb_id;
  std::string m_title_name;
  std::string m_title_description;
  u64 m_title_id;
  u16 m_revision;
};
