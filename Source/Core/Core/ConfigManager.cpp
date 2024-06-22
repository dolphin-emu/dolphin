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

#include "Core/AchievementManager.h"
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
#include "Core/WC24PatchEngine.h"

#include "VideoCommon/HiresTextures.h"

#include "DiscIO/Enums.h"
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
  Config::Load();
}

void SConfig::ResetRunningGameMetadata()
{
  SetRunningGameMetadata("00000000", "", 0, 0, DiscIO::Region::Unknown);
}

void SConfig::SetRunningGameMetadata(const DiscIO::Volume& volume,
                                     const DiscIO::Partition& partition)
{
  if (partition == volume.GetGamePartition())
  {
    SetRunningGameMetadata(volume.GetGameID(), volume.GetGameTDBID(),
                           volume.GetTitleID().value_or(0), volume.GetRevision().value_or(0),
                           volume.GetRegion());
  }
  else
  {
    SetRunningGameMetadata(volume.GetGameID(partition), volume.GetGameTDBID(),
                           volume.GetTitleID(partition).value_or(0),
                           volume.GetRevision(partition).value_or(0), volume.GetRegion());
  }
}

void SConfig::SetRunningGameMetadata(const IOS::ES::TMDReader& tmd, DiscIO::Platform platform)
{
  const u64 tmd_title_id = tmd.GetTitleId();

  // If we're launching a disc game, we want to read the revision from
  // the disc header instead of the TMD. They can differ.
  // (IOS HLE ES calls us with a TMDReader rather than a volume when launching
  // a disc game, because ES has no reason to be accessing the disc directly.)
  if (platform == DiscIO::Platform::WiiWAD ||
      !Core::System::GetInstance().GetDVDInterface().UpdateRunningGameMetadata(tmd_title_id))
  {
    // If not launching a disc game, just read everything from the TMD.
    SetRunningGameMetadata(tmd.GetGameID(), tmd.GetGameTDBID(), tmd_title_id, tmd.GetTitleVersion(),
                           tmd.GetRegion());
  }
}

void SConfig::SetRunningGameMetadata(const std::string& game_id)
{
  SetRunningGameMetadata(game_id, "", 0, 0, DiscIO::Region::Unknown);
}

void SConfig::SetRunningGameMetadata(const std::string& game_id, const std::string& gametdb_id,
                                     u64 title_id, u16 revision, DiscIO::Region region)
{
  const bool was_changed = m_game_id != game_id || m_gametdb_id != gametdb_id ||
                           m_title_id != title_id || m_revision != revision;
  m_game_id = game_id;
  m_gametdb_id = gametdb_id;
  m_title_id = title_id;
  m_revision = revision;

  if (game_id.length() == 6)
  {
    m_debugger_game_id = game_id;
  }
  else if (title_id != 0)
  {
    m_debugger_game_id =
        fmt::format("{:08X}_{:08X}", static_cast<u32>(title_id >> 32), static_cast<u32>(title_id));
  }
  else
  {
    m_debugger_game_id.clear();
  }

  if (!was_changed)
    return;

  if (game_id == "00000000")
  {
    m_title_name.clear();
    m_title_description.clear();
    return;
  }

  AchievementManager::GetInstance().CloseGame();

  const Core::TitleDatabase title_database;
  auto& system = Core::System::GetInstance();
  const DiscIO::Language language = GetLanguageAdjustedForRegion(system.IsWii(), region);
  m_title_name = title_database.GetTitleName(m_gametdb_id, language);
  m_title_description = title_database.Describe(m_gametdb_id, language);
  NOTICE_LOG_FMT(CORE, "Active title: {}", m_title_description);
  Host_TitleChanged();

  const bool is_running_or_starting = Core::IsRunningOrStarting(system);
  if (is_running_or_starting)
    Core::UpdateTitle(system);

  Config::AddLayer(ConfigLoaders::GenerateGlobalGameConfigLoader(game_id, revision));
  Config::AddLayer(ConfigLoaders::GenerateLocalGameConfigLoader(game_id, revision));

  if (is_running_or_starting)
    DolphinAnalytics::Instance().ReportGameStart();
}

void SConfig::OnNewTitleLoad(const Core::CPUThreadGuard& guard)
{
  auto& system = guard.GetSystem();
  if (!Core::IsRunningOrStarting(system))
    return;

  auto& ppc_symbol_db = system.GetPPCSymbolDB();
  if (!ppc_symbol_db.IsEmpty())
  {
    ppc_symbol_db.Clear();
    Host_PPCSymbolsChanged();
  }
  CBoot::LoadMapFromFilename(guard, ppc_symbol_db);
  HLE::Reload(system);
  PatchEngine::Reload();
  HiresTexture::Update();
  WC24PatchEngine::Reload();
}

void SConfig::LoadDefaults()
{
  bBootToPause = false;

  auto& system = Core::System::GetInstance();
  system.SetIsWii(false);

  ResetRunningGameMetadata();
}

// Static method to make a simple game ID for elf/dol files
std::string SConfig::MakeGameID(std::string_view file_name)
{
  size_t lastdot = file_name.find_last_of(".");
  if (lastdot == std::string::npos)
    return "ID-" + std::string(file_name);
  return "ID-" + std::string(file_name.substr(0, lastdot));
}

struct SetGameMetadata
{
  SetGameMetadata(SConfig* config_, Core::System& system_, DiscIO::Region* region_)
      : config(config_), system(system_), region(region_)
  {
  }
  bool operator()(const BootParameters::Disc& disc) const
  {
    *region = disc.volume->GetRegion();
    system.SetIsWii(disc.volume->GetVolumeType() == DiscIO::Platform::WiiDisc);
    config->m_disc_booted_from_game_list = true;
    config->SetRunningGameMetadata(*disc.volume, disc.volume->GetGamePartition());
    return true;
  }

  bool operator()(const BootParameters::Executable& executable) const
  {
    if (!executable.reader->IsValid())
      return false;

    *region = DiscIO::Region::Unknown;
    system.SetIsWii(executable.reader->IsWii());

    // Strip the .elf/.dol file extension and directories before the name
    SplitPath(executable.path, nullptr, &config->m_debugger_game_id, nullptr);

    // Set DOL/ELF game ID appropriately
    std::string executable_path = executable.path;
    constexpr char BACKSLASH = '\\';
    constexpr char FORWARDSLASH = '/';
    std::replace(executable_path.begin(), executable_path.end(), BACKSLASH, FORWARDSLASH);
    config->SetRunningGameMetadata(SConfig::MakeGameID(PathToFileName(executable_path)));

    Host_TitleChanged();

    return true;
  }

  bool operator()(const DiscIO::VolumeWAD& wad) const
  {
    if (!wad.GetTMD().IsValid())
    {
      PanicAlertFmtT("This WAD is not valid.");
      return false;
    }
    if (!IOS::ES::IsChannel(wad.GetTMD().GetTitleId()))
    {
      PanicAlertFmtT("This WAD is not bootable.");
      return false;
    }

    const IOS::ES::TMDReader& tmd = wad.GetTMD();
    *region = tmd.GetRegion();
    system.SetIsWii(true);
    config->SetRunningGameMetadata(tmd, DiscIO::Platform::WiiWAD);

    return true;
  }

  bool operator()(const BootParameters::NANDTitle& nand_title) const
  {
    IOS::HLE::Kernel ios;
    const IOS::ES::TMDReader tmd = ios.GetESCore().FindInstalledTMD(nand_title.id);
    if (!tmd.IsValid() || !IOS::ES::IsChannel(nand_title.id))
    {
      PanicAlertFmtT("This title cannot be booted.");
      return false;
    }

    *region = tmd.GetRegion();
    system.SetIsWii(true);
    config->SetRunningGameMetadata(tmd, DiscIO::Platform::WiiWAD);

    return true;
  }

  bool operator()(const BootParameters::IPL& ipl) const
  {
    *region = ipl.region;
    system.SetIsWii(false);
    Host_TitleChanged();

    return true;
  }

  bool operator()(const BootParameters::DFF& dff) const
  {
    std::unique_ptr<FifoDataFile> dff_file(FifoDataFile::Load(dff.dff_path, true));
    if (!dff_file)
      return false;

    *region = DiscIO::Region::NTSC_U;
    system.SetIsWii(dff_file->GetIsWii());
    Host_TitleChanged();

    return true;
  }

private:
  SConfig* config;
  Core::System& system;
  DiscIO::Region* region;
};

bool SConfig::SetPathsAndGameMetadata(Core::System& system, const BootParameters& boot)
{
  system.SetIsMIOS(false);
  m_disc_booted_from_game_list = false;
  if (!std::visit(SetGameMetadata(this, system, &m_region), boot.parameters))
    return false;

  if (m_region == DiscIO::Region::Unknown)
    m_region = Config::Get(Config::MAIN_FALLBACK_REGION);

  // Set up paths
  const std::string region_dir = Config::GetDirectoryForRegion(Config::ToGameCubeRegion(m_region));
  m_strSRAM = File::GetUserPath(F_GCSRAM_IDX);
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

Common::IniFile SConfig::LoadDefaultGameIni() const
{
  return LoadDefaultGameIni(GetGameID(), m_revision);
}

Common::IniFile SConfig::LoadLocalGameIni() const
{
  return LoadLocalGameIni(GetGameID(), m_revision);
}

Common::IniFile SConfig::LoadGameIni() const
{
  return LoadGameIni(GetGameID(), m_revision);
}

Common::IniFile SConfig::LoadDefaultGameIni(const std::string& id, std::optional<u16> revision)
{
  Common::IniFile game_ini;
  for (const std::string& filename : ConfigLoaders::GetGameIniFilenames(id, revision))
    game_ini.Load(File::GetSysDirectory() + GAMESETTINGS_DIR DIR_SEP + filename, true);
  return game_ini;
}

Common::IniFile SConfig::LoadLocalGameIni(const std::string& id, std::optional<u16> revision)
{
  Common::IniFile game_ini;
  for (const std::string& filename : ConfigLoaders::GetGameIniFilenames(id, revision))
    game_ini.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + filename, true);
  return game_ini;
}

Common::IniFile SConfig::LoadGameIni(const std::string& id, std::optional<u16> revision)
{
  Common::IniFile game_ini;
  for (const std::string& filename : ConfigLoaders::GetGameIniFilenames(id, revision))
    game_ini.Load(File::GetSysDirectory() + GAMESETTINGS_DIR DIR_SEP + filename, true);
  for (const std::string& filename : ConfigLoaders::GetGameIniFilenames(id, revision))
    game_ini.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + filename, true);
  return game_ini;
}

std::string SConfig::GetGameTDBImageRegionCode(bool wii, DiscIO::Region region) const
{
  switch (region)
  {
  case DiscIO::Region::NTSC_J:
    return "JA";
  case DiscIO::Region::NTSC_U:
    return "US";
  case DiscIO::Region::NTSC_K:
    return "KO";
  case DiscIO::Region::PAL:
  {
    const auto user_lang = GetCurrentLanguage(wii);
    switch (user_lang)
    {
    case DiscIO::Language::German:
      return "DE";
    case DiscIO::Language::French:
      return "FR";
    case DiscIO::Language::Spanish:
      return "ES";
    case DiscIO::Language::Italian:
      return "IT";
    case DiscIO::Language::Dutch:
      return "NL";
    case DiscIO::Language::English:
    default:
      return "EN";
    }
  }
  default:
    return "EN";
  }
}
