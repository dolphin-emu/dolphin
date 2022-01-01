// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/ConfigManager.h"

#include <algorithm>
#include <cinttypes>
#include <climits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>

#include <fmt/format.h>

#include "AudioCommon/AudioCommon.h"

#include "Common/Assert.h"
#include "Common/CDUtils.h"
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
#include "Core/TitleDatabase.h"
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
  IniFile ini;
  ini.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));  // load first to not kill unknown stuff

  SaveGeneralSettings(ini);
  SaveCoreSettings(ini);

  ini.Save(File::GetUserPath(F_DOLPHINCONFIG_IDX));

  Config::Save();
}

void SConfig::SaveGeneralSettings(IniFile& ini)
{
  IniFile::Section* general = ini.GetOrCreateSection("General");

  // General
  general->Set("ShowLag", m_ShowLag);
  general->Set("ShowFrameCount", m_ShowFrameCount);

  // ISO folders
  // Clear removed folders
  int oldPaths;
  int numPaths = (int)m_ISOFolder.size();
  general->Get("ISOPaths", &oldPaths, 0);
  for (int i = numPaths; i < oldPaths; i++)
  {
    ini.DeleteKey("General", fmt::format("ISOPath{}", i));
  }

  general->Set("ISOPaths", numPaths);
  for (int i = 0; i < numPaths; i++)
  {
    general->Set(fmt::format("ISOPath{}", i), m_ISOFolder[i]);
  }

  general->Set("WirelessMac", m_WirelessMac);

#ifndef _WIN32
  general->Set("GDBSocket", gdb_socket);
#endif
  general->Set("GDBPort", iGDBPort);
}

void SConfig::SaveCoreSettings(IniFile& ini)
{
  IniFile::Section* core = ini.GetOrCreateSection("Core");

  core->Set("SkipIPL", bHLE_BS2);
  core->Set("TimingVariance", iTimingVariance);
  core->Set("CPUCore", cpu_core);
  core->Set("Fastmem", bFastmem);
  core->Set("CPUThread", bCPUThread);
  core->Set("SyncOnSkipIdle", bSyncGPUOnSkipIdleHack);
  core->Set("SyncGPU", bSyncGPU);
  core->Set("SyncGpuMaxDistance", iSyncGpuMaxDistance);
  core->Set("SyncGpuMinDistance", iSyncGpuMinDistance);
  core->Set("SyncGpuOverclock", fSyncGpuOverclock);
  core->Set("FloatExceptions", bFloatExceptions);
  core->Set("DivByZeroExceptions", bDivideByZeroExceptions);
  core->Set("FPRF", bFPRF);
  core->Set("AccurateNaNs", bAccurateNaNs);
  core->Set("SelectedLanguage", SelectedLanguage);
  core->Set("OverrideRegionSettings", bOverrideRegionSettings);
  core->Set("AgpCartAPath", m_strGbaCartA);
  core->Set("AgpCartBPath", m_strGbaCartB);
  core->Set("SlotA", m_EXIDevice[0]);
  core->Set("SlotB", m_EXIDevice[1]);
  core->Set("SerialPort1", m_EXIDevice[2]);
  core->Set("BBA_MAC", m_bba_mac);
  core->Set("BBA_XLINK_IP", m_bba_xlink_ip);
  core->Set("BBA_XLINK_CHAT_OSD", m_bba_xlink_chat_osd);
  for (int i = 0; i < SerialInterface::MAX_SI_CHANNELS; ++i)
  {
    core->Set(fmt::format("SIDevice{}", i), m_SIDevice[i]);
    core->Set(fmt::format("AdapterRumble{}", i), m_AdapterRumble[i]);
    core->Set(fmt::format("SimulateKonga{}", i), m_AdapterKonga[i]);
  }
  core->Set("WiiSDCard", m_WiiSDCard);
  core->Set("WiiKeyboard", m_WiiKeyboard);
  core->Set("WiimoteContinuousScanning", m_WiimoteContinuousScanning);
  core->Set("WiimoteEnableSpeaker", m_WiimoteEnableSpeaker);
  core->Set("WiimoteControllerInterface", connect_wiimotes_for_ciface);
  core->Set("RunCompareServer", bRunCompareServer);
  core->Set("RunCompareClient", bRunCompareClient);
  core->Set("MMU", bMMU);
  core->Set("EmulationSpeed", m_EmulationSpeed);
  core->Set("GPUDeterminismMode", m_strGPUDeterminismMode);
  core->Set("PerfMapDir", m_perfDir);
  core->Set("EnableCustomRTC", bEnableCustomRTC);
  core->Set("CustomRTCValue", m_customRTCValue);
}

void SConfig::LoadSettings()
{
  Config::Load();

  INFO_LOG_FMT(BOOT, "Loading Settings from {}", File::GetUserPath(F_DOLPHINCONFIG_IDX));
  IniFile ini;
  ini.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));

  LoadGeneralSettings(ini);
  LoadCoreSettings(ini);
}

void SConfig::LoadGeneralSettings(IniFile& ini)
{
  IniFile::Section* general = ini.GetOrCreateSection("General");

  general->Get("ShowLag", &m_ShowLag, false);
  general->Get("ShowFrameCount", &m_ShowFrameCount, false);
#ifndef _WIN32
  general->Get("GDBSocket", &gdb_socket, "");
#endif
  general->Get("GDBPort", &(iGDBPort), -1);

  m_ISOFolder.clear();
  int numISOPaths;

  if (general->Get("ISOPaths", &numISOPaths, 0))
  {
    for (int i = 0; i < numISOPaths; i++)
    {
      std::string tmpPath;
      general->Get(fmt::format("ISOPath{}", i), &tmpPath, "");
      m_ISOFolder.push_back(std::move(tmpPath));
    }
  }

  general->Get("WirelessMac", &m_WirelessMac);
}

void SConfig::LoadCoreSettings(IniFile& ini)
{
  IniFile::Section* core = ini.GetOrCreateSection("Core");

  core->Get("SkipIPL", &bHLE_BS2, true);
#ifdef _M_X86
  core->Get("CPUCore", &cpu_core, PowerPC::CPUCore::JIT64);
#elif _M_ARM_64
  core->Get("CPUCore", &cpu_core, PowerPC::CPUCore::JITARM64);
#else
  core->Get("CPUCore", &cpu_core, PowerPC::CPUCore::Interpreter);
#endif
  core->Get("JITFollowBranch", &bJITFollowBranch, true);
  core->Get("Fastmem", &bFastmem, true);
  core->Get("TimingVariance", &iTimingVariance, 40);
  core->Get("CPUThread", &bCPUThread, true);
  core->Get("SyncOnSkipIdle", &bSyncGPUOnSkipIdleHack, true);
  core->Get("SelectedLanguage", &SelectedLanguage,
            DiscIO::ToGameCubeLanguage(Config::GetDefaultLanguage()));
  core->Get("OverrideRegionSettings", &bOverrideRegionSettings, false);
  core->Get("AgpCartAPath", &m_strGbaCartA);
  core->Get("AgpCartBPath", &m_strGbaCartB);
  core->Get("SlotA", (int*)&m_EXIDevice[0], ExpansionInterface::EXIDEVICE_MEMORYCARDFOLDER);
  core->Get("SlotB", (int*)&m_EXIDevice[1], ExpansionInterface::EXIDEVICE_NONE);
  core->Get("SerialPort1", (int*)&m_EXIDevice[2], ExpansionInterface::EXIDEVICE_NONE);
  core->Get("BBA_MAC", &m_bba_mac);
  core->Get("BBA_XLINK_IP", &m_bba_xlink_ip, "127.0.0.1");
  core->Get("BBA_XLINK_CHAT_OSD", &m_bba_xlink_chat_osd, true);
  for (size_t i = 0; i < std::size(m_SIDevice); ++i)
  {
    core->Get(fmt::format("SIDevice{}", i), &m_SIDevice[i],
              (i == 0) ? SerialInterface::SIDEVICE_GC_CONTROLLER : SerialInterface::SIDEVICE_NONE);
    core->Get(fmt::format("AdapterRumble{}", i), &m_AdapterRumble[i], true);
    core->Get(fmt::format("SimulateKonga{}", i), &m_AdapterKonga[i], false);
  }
  core->Get("WiiSDCard", &m_WiiSDCard, true);
  core->Get("WiiKeyboard", &m_WiiKeyboard, false);
  core->Get("WiimoteContinuousScanning", &m_WiimoteContinuousScanning, false);
  core->Get("WiimoteEnableSpeaker", &m_WiimoteEnableSpeaker, false);
  core->Get("WiimoteControllerInterface", &connect_wiimotes_for_ciface, false);
  core->Get("RunCompareServer", &bRunCompareServer, false);
  core->Get("RunCompareClient", &bRunCompareClient, false);
  core->Get("MMU", &bMMU, bMMU);
  core->Get("BBDumpPort", &iBBDumpPort, -1);
  core->Get("SyncGPU", &bSyncGPU, false);
  core->Get("SyncGpuMaxDistance", &iSyncGpuMaxDistance, 200000);
  core->Get("SyncGpuMinDistance", &iSyncGpuMinDistance, -200000);
  core->Get("SyncGpuOverclock", &fSyncGpuOverclock, 1.0f);
  core->Get("FastDiscSpeed", &bFastDiscSpeed, false);
  core->Get("LowDCBZHack", &bLowDCBZHack, false);
  core->Get("FloatExceptions", &bFloatExceptions, false);
  core->Get("DivByZeroExceptions", &bDivideByZeroExceptions, false);
  core->Get("FPRF", &bFPRF, false);
  core->Get("AccurateNaNs", &bAccurateNaNs, false);
  core->Get("DisableICache", &bDisableICache, false);
  core->Get("EmulationSpeed", &m_EmulationSpeed, 1.0f);
  core->Get("GPUDeterminismMode", &m_strGPUDeterminismMode, "auto");
  core->Get("PerfMapDir", &m_perfDir, "");
  core->Get("EnableCustomRTC", &bEnableCustomRTC, false);
  // Default to seconds between 1.1.1970 and 1.1.2000
  core->Get("CustomRTCValue", &m_customRTCValue, 946684800);
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
      !DVDInterface::UpdateRunningGameMetadata(tmd_title_id))
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

  const Core::TitleDatabase title_database;
  const DiscIO::Language language = GetLanguageAdjustedForRegion(bWii, region);
  m_title_name = title_database.GetTitleName(m_gametdb_id, language);
  m_title_description = title_database.Describe(m_gametdb_id, language);
  NOTICE_LOG_FMT(CORE, "Active title: {}", m_title_description);
  Host_TitleChanged();

  Config::AddLayer(ConfigLoaders::GenerateGlobalGameConfigLoader(game_id, revision));
  Config::AddLayer(ConfigLoaders::GenerateLocalGameConfigLoader(game_id, revision));

  if (Core::IsRunning())
    DolphinAnalytics::Instance().ReportGameStart();
}

void SConfig::OnNewTitleLoad()
{
  if (!Core::IsRunning())
    return;

  if (!g_symbolDB.IsEmpty())
  {
    g_symbolDB.Clear();
    Host_NotifyMapLoaded();
  }
  CBoot::LoadMapFromFilename();
  HLE::Reload();
  PatchEngine::Reload();
  HiresTexture::Update();
}

void SConfig::LoadDefaults()
{
  bAutomaticStart = false;
  bBootToPause = false;

  iGDBPort = -1;
#ifndef _WIN32
  gdb_socket = "";
#endif

  cpu_core = PowerPC::DefaultCPUCore();
  iTimingVariance = 40;
  bCPUThread = false;
  bSyncGPUOnSkipIdleHack = true;
  bRunCompareServer = false;
  bFastmem = true;
  bFloatExceptions = false;
  bDivideByZeroExceptions = false;
  bFPRF = false;
  bAccurateNaNs = false;
  bDisableICache = false;
  bMMU = false;
  bLowDCBZHack = false;
  iBBDumpPort = -1;
  bSyncGPU = false;
  bFastDiscSpeed = false;
  SelectedLanguage = 0;
  bOverrideRegionSettings = false;
  bWii = false;

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

// The reason we need this function is because some memory card code
// expects to get a non-NTSC-K region even if we're emulating an NTSC-K Wii.
DiscIO::Region SConfig::ToGameCubeRegion(DiscIO::Region region)
{
  if (region != DiscIO::Region::NTSC_K)
    return region;

  // GameCube has no NTSC-K region. No choice of replacement value is completely
  // non-arbitrary, but let's go with NTSC-J since Korean GameCubes are NTSC-J.
  return DiscIO::Region::NTSC_J;
}

const char* SConfig::GetDirectoryForRegion(DiscIO::Region region)
{
  if (region == DiscIO::Region::Unknown)
    region = ToGameCubeRegion(GetFallbackRegion());

  switch (region)
  {
  case DiscIO::Region::NTSC_J:
    return JAP_DIR;

  case DiscIO::Region::NTSC_U:
    return USA_DIR;

  case DiscIO::Region::PAL:
    return EUR_DIR;

  case DiscIO::Region::NTSC_K:
    ASSERT_MSG(BOOT, false, "NTSC-K is not a valid GameCube region");
    return JAP_DIR;  // See ToGameCubeRegion

  default:
    ASSERT_MSG(BOOT, false, "Default case should not be reached");
    return EUR_DIR;
  }
}

std::string SConfig::GetBootROMPath(const std::string& region_directory) const
{
  const std::string path =
      File::GetUserPath(D_GCUSER_IDX) + DIR_SEP + region_directory + DIR_SEP GC_IPL;
  if (!File::Exists(path))
    return File::GetSysDirectory() + GC_SYS_DIR + DIR_SEP + region_directory + DIR_SEP GC_IPL;
  return path;
}

struct SetGameMetadata
{
  SetGameMetadata(SConfig* config_, DiscIO::Region* region_) : config(config_), region(region_) {}
  bool operator()(const BootParameters::Disc& disc) const
  {
    *region = disc.volume->GetRegion();
    config->bWii = disc.volume->GetVolumeType() == DiscIO::Platform::WiiDisc;
    config->m_disc_booted_from_game_list = true;
    config->SetRunningGameMetadata(*disc.volume, disc.volume->GetGamePartition());
    return true;
  }

  bool operator()(const BootParameters::Executable& executable) const
  {
    if (!executable.reader->IsValid())
      return false;

    *region = DiscIO::Region::Unknown;
    config->bWii = executable.reader->IsWii();

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
    config->bWii = true;
    config->SetRunningGameMetadata(tmd, DiscIO::Platform::WiiWAD);

    return true;
  }

  bool operator()(const BootParameters::NANDTitle& nand_title) const
  {
    IOS::HLE::Kernel ios;
    const IOS::ES::TMDReader tmd = ios.GetES()->FindInstalledTMD(nand_title.id);
    if (!tmd.IsValid() || !IOS::ES::IsChannel(nand_title.id))
    {
      PanicAlertFmtT("This title cannot be booted.");
      return false;
    }

    *region = tmd.GetRegion();
    config->bWii = true;
    config->SetRunningGameMetadata(tmd, DiscIO::Platform::WiiWAD);

    return true;
  }

  bool operator()(const BootParameters::IPL& ipl) const
  {
    *region = ipl.region;
    config->bWii = false;
    Host_TitleChanged();

    return true;
  }

  bool operator()(const BootParameters::DFF& dff) const
  {
    std::unique_ptr<FifoDataFile> dff_file(FifoDataFile::Load(dff.dff_path, true));
    if (!dff_file)
      return false;

    *region = DiscIO::Region::NTSC_U;
    config->bWii = dff_file->GetIsWii();
    Host_TitleChanged();

    return true;
  }

private:
  SConfig* config;
  DiscIO::Region* region;
};

bool SConfig::SetPathsAndGameMetadata(const BootParameters& boot)
{
  m_is_mios = false;
  m_disc_booted_from_game_list = false;
  if (!std::visit(SetGameMetadata(this, &m_region), boot.parameters))
    return false;

  if (m_region == DiscIO::Region::Unknown)
    m_region = GetFallbackRegion();

  // Set up paths
  const std::string region_dir = GetDirectoryForRegion(ToGameCubeRegion(m_region));
  m_strSRAM = File::GetUserPath(F_GCSRAM_IDX);
  m_strBootROM = GetBootROMPath(region_dir);

  return true;
}

DiscIO::Region SConfig::GetFallbackRegion()
{
  return Config::Get(Config::MAIN_FALLBACK_REGION);
}

DiscIO::Language SConfig::GetCurrentLanguage(bool wii) const
{
  DiscIO::Language language;
  if (wii)
    language = static_cast<DiscIO::Language>(Config::Get(Config::SYSCONF_LANGUAGE));
  else
    language = DiscIO::FromGameCubeLanguage(SConfig::GetInstance().SelectedLanguage);

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

  if (!bOverrideRegionSettings)
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
