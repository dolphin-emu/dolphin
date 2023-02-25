// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/ConfigManager.h"

#include <algorithm>
#include <climits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>

#include <Core/Core.h>

#include <fmt/format.h>

#include "AudioCommon/AudioCommon.h"

#include "Common/Assert.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"
#include "Common/Version.h"

#include "Core/Boot/Boot.h"
#include "Core/CommonTitles.h"
#include "Core/Config/DefaultLocale.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/ConfigLoaders/GameConfigLoader.h"
#include "Core/Core.h"
#include "Core/DolphinAnalytics.h"
#include "Core/FifoPlayer/FifoDataFile.h"
#include "Core/HLE/HLE.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/EXI/EXI_Device.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/Host.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/PatchEngine.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"
#include "Core/TitleDatabase.h"
#include "VideoCommon/HiresTextures.h"

#include "DiscIO/Enums.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeWad.h"

SConfig* SConfig::m_Instance;

SConfig::SConfig()
{
  LoadDefaults();
  // Make sure we have log manager
  LoadSettings();
}

void SConfig::Init()
{
  m_Instance = new SConfig;
}

void SConfig::Shutdown()
{
  delete m_Instance;
  m_Instance = nullptr;
}

SConfig::~SConfig()
{
  SaveSettings();
}

void SConfig::SaveSettings()
{
  NOTICE_LOG_FMT(BOOT, "Saving settings to {}", File::GetUserPath(F_DOLPHINCONFIG_IDX));
  Config::Save();
}

void SConfig::LoadSettings()
{
  INFO_LOG_FMT(BOOT, "Loading Settings from {}", File::GetUserPath(F_DOLPHINCONFIG_IDX));
  m_strBootROM = Config::GetBootROMPath(region_dir);

  return true;
}

DiscIO::Language SConfig::GetCurrentLanguage(bool wii) const
{
  DiscIO::Language language;
  if (wii)
    language = static_cast<DiscIO::Language>(Config::Get(Config::SYSCONF_LANGUAGE));
  else
    language = DiscIO::FromGameCubeLanguage(Config::Get(Config::MAIN_GC_LANGUAGE));

  // Get rid of invalid values (probably doesn't matter, but might as well do it)
  if (language > DiscIO::Language::Unknown || language < DiscIO::Language::Japanese)
    language = DiscIO::Language::Unknown;
  return language;
}

DiscIO::Language SConfig::GetLanguageAdjustedForRegion(bool wii, DiscIO::Region region) const
{
  const DiscIO::Language language = GetCurrentLanguage(wii);

  if (!wii && region == DiscIO::Region::NTSC_K)
    region = DiscIO::Region::NTSC_J;  // NTSC-K only exists on Wii, so use a fallback

  if (!wii && region == DiscIO::Region::NTSC_J && language == DiscIO::Language::English)
    return DiscIO::Language::Japanese;  // English and Japanese both use the value 0 in GC SRAM

  if (!Config::Get(Config::MAIN_OVERRIDE_REGION_SETTINGS))
  {
    if (region == DiscIO::Region::NTSC_J)
      return DiscIO::Language::Japanese;

    if (region == DiscIO::Region::NTSC_U && language != DiscIO::Language::English &&
        (!wii || (language != DiscIO::Language::French && language != DiscIO::Language::Spanish)))
    {
      return DiscIO::Language::English;
    }

    if (region == DiscIO::Region::PAL &&
        (language < DiscIO::Language::English || language > DiscIO::Language::Dutch))
    {
      return DiscIO::Language::English;
    }

    if (region == DiscIO::Region::NTSC_K)
      return DiscIO::Language::Korean;
  }

  return language;
}

IniFile SConfig::LoadDefaultGameIni() const
{
  return LoadDefaultGameIni(GetGameID(), m_revision);
}

IniFile SConfig::LoadLocalGameIni() const
{
  return LoadLocalGameIni(GetGameID(), m_revision);
}

IniFile SConfig::LoadGameIni() const
{
  return LoadGameIni(GetGameID(), m_revision);
}

IniFile SConfig::LoadDefaultGameIni(const std::string& id, std::optional<u16> revision)
{
  IniFile game_ini;
  for (const std::string& filename : ConfigLoaders::GetGameIniFilenames(id, revision))
    game_ini.Load(File::GetSysDirectory() + GAMESETTINGS_DIR DIR_SEP + filename, true);
  return game_ini;
}

IniFile SConfig::LoadLocalGameIni(const std::string& id, std::optional<u16> revision)
{
  IniFile game_ini;
  for (const std::string& filename : ConfigLoaders::GetGameIniFilenames(id, revision))
    game_ini.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + filename, true);
  return game_ini;
}

IniFile SConfig::LoadGameIni(const std::string& id, std::optional<u16> revision)
{
  IniFile game_ini;
  for (const std::string& filename : ConfigLoaders::GetGameIniFilenames(id, revision))
    game_ini.Load(File::GetSysDirectory() + GAMESETTINGS_DIR DIR_SEP + filename, true);
  for (const std::string& filename : ConfigLoaders::GetGameIniFilenames(id, revision))
    game_ini.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + filename, true);
  return game_ini;
}
