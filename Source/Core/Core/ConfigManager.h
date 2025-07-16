// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <string_view>

#include "Common/CommonTypes.h"

namespace Common
{
class IniFile;
}

namespace Core
{
class CPUThreadGuard;
class System;
}  // namespace Core

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

static constexpr std::string_view DEFAULT_GAME_ID = "00000000";

struct SConfig
{
  // Settings
  bool bBootToPause = false;

  bool bJITNoBlockCache = false;
  bool bJITNoBlockLinking = false;

  bool bCopyWiiSaveNetplay = true;

  DiscIO::Region m_region;

  // files
  std::string m_strBootROM;
  std::string m_strSRAM;
  std::string m_debugger_game_id;

  // TODO: remove this as soon as the ticket view hack in IOS/ES/Views is dropped.
  bool m_disc_booted_from_game_list = false;

  const std::string GetGameID() const;
  const std::string GetGameTDBID() const;
  const std::string GetTitleName() const;
  const std::string GetTitleDescription() const;
  u64 GetTitleID() const;
  u16 GetRevision() const;
  void ResetRunningGameMetadata();
  void SetRunningGameMetadata(const DiscIO::Volume& volume, const DiscIO::Partition& partition);
  void SetRunningGameMetadata(const IOS::ES::TMDReader& tmd, DiscIO::Platform platform);
  void SetRunningGameMetadata(const std::string& game_id);

  // Triggered when Dolphin loads a title directly
  // Reloads title-specific map files, patches, etc.
  static void OnTitleDirectlyBooted(const Core::CPUThreadGuard& guard);

  // Direct title change from ES (Wii system)
  // Wii titles will still hit OnTitleDirectlyBooted
  // but GC titles will never see this call
  static void OnESTitleChanged();

  void LoadDefaults();
  static std::string MakeGameID(std::string_view file_name);
  bool SetPathsAndGameMetadata(Core::System& system, const BootParameters& boot);
  DiscIO::Language GetCurrentLanguage(bool wii) const;
  DiscIO::Language GetLanguageAdjustedForRegion(bool wii, DiscIO::Region region) const;
  std::string GetGameTDBImageRegionCode(bool wii, DiscIO::Region region) const;

  Common::IniFile LoadDefaultGameIni() const;
  Common::IniFile LoadLocalGameIni() const;
  Common::IniFile LoadGameIni() const;

  static Common::IniFile LoadDefaultGameIni(std::string_view id, std::optional<u16> revision);
  static Common::IniFile LoadLocalGameIni(std::string_view id, std::optional<u16> revision);
  static Common::IniFile LoadGameIni(std::string_view id, std::optional<u16> revision);

  SConfig(const SConfig&) = delete;
  SConfig& operator=(const SConfig&) = delete;
  SConfig(SConfig&&) = delete;
  SConfig& operator=(SConfig&&) = delete;

  // Save settings
  void SaveSettings();

  // Load settings
  void LoadSettings();

  static void ResetAllSettings();

  // Return the permanent and somewhat globally used instance of this struct
  static SConfig& GetInstance() { return (*m_Instance); }
  static void Init();
  static void Shutdown();

private:
  SConfig();
  ~SConfig();

  static void ReloadTextures(Core::System& system);

  void SetRunningGameMetadata(const std::string& game_id, const std::string& gametdb_id,
                              u64 title_id, u16 revision, DiscIO::Region region);

  static SConfig* m_Instance;
  mutable std::recursive_mutex m_metadata_lock;

  std::string m_game_id;
  std::string m_gametdb_id;
  std::string m_title_name;
  std::string m_title_description;
  u64 m_title_id;
  u16 m_revision;
};
