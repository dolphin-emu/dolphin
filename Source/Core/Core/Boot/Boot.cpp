// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#include "Core/Boot/Boot.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <numeric>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include <zlib.h>

#include "Common/Align.h"
#include "Common/CDUtils.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

#include "Core/ARBruteForcer.h"
#include "Core/Boot/DolReader.h"
#include "Core/Boot/ElfReader.h"
#include "Core/CommonTitles.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/ConfigManager.h"
#include "Core/FifoPlayer/FifoPlayer.h"
#include "Core/HLE/HLE.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/EXI/EXI_DeviceIPL.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/VideoInterface.h"
#include "Core/HideObjectEngine.h"
#include "Core/Host.h"
#include "Core/IOS/IOS.h"
#include "Core/PatchEngine.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"

#include "DiscIO/Enums.h"
#include "DiscIO/Volume.h"

BootParameters::BootParameters(Parameters&& parameters_) : parameters(std::move(parameters_))
{
}

std::unique_ptr<BootParameters> BootParameters::GenerateFromFile(const std::string& path)
{
  const bool is_drive = cdio_is_cdrom(path);
  // Check if the file exist, we may have gotten it from a --elf command line
  // that gave an incorrect file name
  if (!is_drive && !File::Exists(path))
  {
    PanicAlertT("The specified file \"%s\" does not exist", path.c_str());
    return {};
  }

  std::string extension;
  SplitPath(path, nullptr, nullptr, &extension);
  std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

  static const std::unordered_set<std::string> disc_image_extensions = {
      {".gcm", ".iso", ".tgc", ".wbfs", ".ciso", ".gcz", ".dol", ".elf"}};
  if (disc_image_extensions.find(extension) != disc_image_extensions.end() || is_drive)
  {
    std::unique_ptr<DiscIO::Volume> volume = DiscIO::CreateVolumeFromFilename(path);
    if (volume)
      return std::make_unique<BootParameters>(Disc{path, std::move(volume)});

    if (extension == ".elf")
      return std::make_unique<BootParameters>(Executable{path, std::make_unique<ElfReader>(path)});

    if (extension == ".dol")
      return std::make_unique<BootParameters>(Executable{path, std::make_unique<DolReader>(path)});

    if (is_drive)
    {
      PanicAlertT("Could not read \"%s\". "
                  "There is no disc in the drive or it is not a GameCube/Wii backup. "
                  "Please note that Dolphin cannot play games directly from the original "
                  "GameCube and Wii discs.",
                  path.c_str());
    }
    else
    {
      PanicAlertT("\"%s\" is an invalid GCM/ISO file, or is not a GC/Wii ISO.", path.c_str());
    }
    return {};
  }

  if (extension == ".dff")
    return std::make_unique<BootParameters>(DFF{path});

  if (extension == ".wad")
    return std::make_unique<BootParameters>(DiscIO::WiiWAD{path});

  PanicAlertT("Could not recognize file %s", path.c_str());
  return {};
}

BootParameters::IPL::IPL(DiscIO::Region region_) : region(region_)
{
  const std::string directory = SConfig::GetInstance().GetDirectoryForRegion(region);
  path = SConfig::GetInstance().GetBootROMPath(directory);
}

BootParameters::IPL::IPL(DiscIO::Region region_, Disc&& disc_) : IPL(region_)
{
  disc = std::move(disc_);
}

// Inserts a disc into the emulated disc drive and returns a pointer to it.
// The returned pointer must only be used while we are still booting,
// because DVDThread can do whatever it wants to the disc after that.
static const DiscIO::Volume* SetDisc(std::unique_ptr<DiscIO::Volume> volume)
{
  const DiscIO::Volume* pointer = volume.get();
  DVDInterface::SetDisc(std::move(volume));
  return pointer;
}

bool CBoot::DVDRead(const DiscIO::Volume& volume, u64 dvd_offset, u32 output_address, u32 length,
                    const DiscIO::Partition& partition)
{
  std::vector<u8> buffer(length);
  if (!volume.Read(dvd_offset, length, buffer.data(), partition))
    return false;
  Memory::CopyToEmu(output_address, buffer.data(), length);
  return true;
}

void CBoot::UpdateDebugger_MapLoaded()
{
  Host_NotifyMapLoaded();
}

// Get map file paths for the active title.
bool CBoot::FindMapFile(std::string* existing_map_file, std::string* writable_map_file)
{
  const std::string& game_id = SConfig::GetInstance().m_debugger_game_id;

  if (writable_map_file)
    *writable_map_file = File::GetUserPath(D_MAPS_IDX) + game_id + ".map";

  bool found = false;
  static const std::string maps_directories[] = {File::GetUserPath(D_MAPS_IDX),
                                                 File::GetSysDirectory() + MAPS_DIR DIR_SEP};
  for (size_t i = 0; !found && i < ArraySize(maps_directories); ++i)
  {
    std::string path = maps_directories[i] + game_id + ".map";
    if (File::Exists(path))
    {
      found = true;
      if (existing_map_file)
        *existing_map_file = path;
    }
  }

  return found;
}

bool CBoot::LoadMapFromFilename()
{
  std::string strMapFilename;
  bool found = FindMapFile(&strMapFilename, nullptr);
  if (found && g_symbolDB.LoadMap(strMapFilename))
  {
    UpdateDebugger_MapLoaded();
    return true;
  }

  return false;
}

// If ipl.bin is not found, this function does *some* of what BS1 does:
// loading IPL(BS2) and jumping to it.
// It does not initialize the hardware or anything else like BS1 does.
bool CBoot::Load_BS2(const std::string& boot_rom_filename)
{
  // CRC32 hashes of the IPL file; including source where known
  // https://forums.dolphin-emu.org/Thread-unknown-hash-on-ipl-bin?pid=385344#pid385344
  constexpr u32 USA_v1_0 = 0x6D740AE7;
  // https://forums.dolphin-emu.org/Thread-unknown-hash-on-ipl-bin?pid=385334#pid385334
  constexpr u32 USA_v1_1 = 0xD5E6FEEA;
  // https://forums.dolphin-emu.org/Thread-unknown-hash-on-ipl-bin?pid=385399#pid385399
  constexpr u32 USA_v1_2 = 0x86573808;
  // GameCubes sold in Brazil have this IPL. Same as USA v1.2 but localized
  constexpr u32 BRA_v1_0 = 0x667D0B64;
  // Redump
  constexpr u32 JAP_v1_0 = 0x6DAC1F2A;
  // https://bugs.dolphin-emu.org/issues/8936
  constexpr u32 JAP_v1_1 = 0xD235E3F9;
  constexpr u32 JAP_v1_2 = 0x8BDABBD4;
  // Redump
  constexpr u32 PAL_v1_0 = 0x4F319F43;
  // https://forums.dolphin-emu.org/Thread-ipl-with-unknown-hash-dd8cab7c-problem-caused-by-my-pal-gamecube-bios?pid=435463#pid435463
  constexpr u32 PAL_v1_1 = 0xDD8CAB7C;
  // Redump
  constexpr u32 PAL_v1_2 = 0xAD1B7F16;

  // Load the whole ROM dump
  std::string data;
  if (!File::ReadFileToString(boot_rom_filename, data))
    return false;

  // Use zlibs crc32 implementation to compute the hash
  u32 ipl_hash = crc32(0L, Z_NULL, 0);
  ipl_hash = crc32(ipl_hash, (const Bytef*)data.data(), (u32)data.size());
  DiscIO::Region ipl_region;
  switch (ipl_hash)
  {
  case USA_v1_0:
  case USA_v1_1:
  case USA_v1_2:
  case BRA_v1_0:
    ipl_region = DiscIO::Region::NTSC_U;
    break;
  case JAP_v1_0:
  case JAP_v1_1:
  case JAP_v1_2:
    ipl_region = DiscIO::Region::NTSC_J;
    break;
  case PAL_v1_0:
  case PAL_v1_1:
  case PAL_v1_2:
    ipl_region = DiscIO::Region::PAL;
    break;
  default:
    PanicAlertT("IPL with unknown hash %x", ipl_hash);
    ipl_region = DiscIO::Region::UNKNOWN_REGION;
    break;
  }

  const DiscIO::Region boot_region = SConfig::GetInstance().m_region;
  if (ipl_region != DiscIO::Region::UNKNOWN_REGION && boot_region != ipl_region)
    PanicAlertT("%s IPL found in %s directory. The disc might not be recognized",
                SConfig::GetDirectoryForRegion(ipl_region),
                SConfig::GetDirectoryForRegion(boot_region));

  // Run the descrambler over the encrypted section containing BS1/BS2
  ExpansionInterface::CEXIIPL::Descrambler((u8*)data.data() + 0x100, 0x1AFE00);

  // TODO: Execution is supposed to start at 0xFFF00000, not 0x81200000;
  // copying the initial boot code to 0x81200000 is a hack.
  // For now, HLE the first few instructions and start at 0x81200150
  // to work around this.
  Memory::CopyToEmu(0x01200000, data.data() + 0x100, 0x700);
  Memory::CopyToEmu(0x01300000, data.data() + 0x820, 0x1AFE00);

  PowerPC::ppcState.gpr[3] = 0xfff0001f;
  PowerPC::ppcState.gpr[4] = 0x00002030;
  PowerPC::ppcState.gpr[5] = 0x0000009c;

  UReg_MSR& m_MSR = ((UReg_MSR&)PowerPC::ppcState.msr);
  m_MSR.FP = 1;
  m_MSR.DR = 1;
  m_MSR.IR = 1;

  PowerPC::ppcState.spr[SPR_HID0] = 0x0011c464;
  PowerPC::ppcState.spr[SPR_IBAT3U] = 0xfff0001f;
  PowerPC::ppcState.spr[SPR_IBAT3L] = 0xfff00001;
  PowerPC::ppcState.spr[SPR_DBAT3U] = 0xfff0001f;
  PowerPC::ppcState.spr[SPR_DBAT3L] = 0xfff00001;
  SetupBAT(/*is_wii*/ false);

  PC = 0x81200150;
  return true;
}

static void SetDefaultDisc()
{
  const SConfig& config = SConfig::GetInstance();
  if (!config.m_strDefaultISO.empty())
    SetDisc(DiscIO::CreateVolumeFromFilename(config.m_strDefaultISO));
}

// Third boot step after BootManager and Core. See Call schedule in BootManager.cpp
bool CBoot::BootUp(std::unique_ptr<BootParameters> boot)
{
  SConfig& config = SConfig::GetInstance();

  g_symbolDB.Clear();

  // PAL Wii uses NTSC framerate and linecount in 60Hz modes
  VideoInterface::Preset(DiscIO::IsNTSC(config.m_region) ||
                         (config.bWii && Config::Get(Config::SYSCONF_PAL60)));

  struct BootTitle
  {
    BootTitle() : config(SConfig::GetInstance()) {}
    bool operator()(BootParameters::Disc& disc) const
    {
      NOTICE_LOG(BOOT, "Booting from disc: %s", disc.path.c_str());
      const DiscIO::Volume* volume = SetDisc(std::move(disc.volume));

      if (!volume)
        return false;

      if (!EmulatedBS2(config.bWii, *volume))
        return false;

      // Try to load the symbol map if there is one, and then scan it for
      // and eventually replace code
      if (LoadMapFromFilename())
        HLE::PatchFunctions();

      return true;
    }

    bool operator()(const BootParameters::Executable& executable) const
    {
      NOTICE_LOG(BOOT, "Booting from executable: %s", executable.path.c_str());

      if (!executable.reader->IsValid())
        return false;

      if (!executable.reader->LoadIntoMemory())
      {
        PanicAlertT("Failed to load the executable to memory.");
        return false;
      }

      SetDefaultDisc();

      SetupMSR();
      SetupBAT(config.bWii);

      if (config.bWii)
      {
        HID4.SBE = 1;
        // Because there is no TMD to get the requested system (IOS) version from,
        // we default to IOS58, which is the version used by the Homebrew Channel.
        SetupWiiMemory();
        IOS::HLE::GetIOS()->BootIOS(Titles::IOS(58));
      }
      else
      {
        SetupGCMemory();
      }

      PC = executable.reader->GetEntryPoint();

      if (executable.reader->LoadSymbols() || LoadMapFromFilename())
      {
        UpdateDebugger_MapLoaded();
        HLE::PatchFunctions();
      }
      return true;
    }

    bool operator()(const DiscIO::WiiWAD& wad) const
    {
      SetDefaultDisc();
      return Boot_WiiWAD(wad);
    }

    bool operator()(const BootParameters::NANDTitle& nand_title) const
    {
      SetDefaultDisc();
      return BootNANDTitle(nand_title.id);
    }

    bool operator()(const BootParameters::IPL& ipl) const
    {
      NOTICE_LOG(BOOT, "Booting GC IPL: %s", ipl.path.c_str());
      if (!File::Exists(ipl.path))
      {
        if (ipl.disc)
          PanicAlertT("Cannot start the game, because the GC IPL could not be found.");
        else
          PanicAlertT("Cannot find the GC IPL.");
        return false;
      }

      if (!Load_BS2(ipl.path))
        return false;

      if (ipl.disc)
      {
        NOTICE_LOG(BOOT, "Inserting disc: %s", ipl.disc->path.c_str());
        SetDisc(DiscIO::CreateVolumeFromFilename(ipl.disc->path));
      }

      if (LoadMapFromFilename())
        HLE::PatchFunctions();

      return true;
    }

    bool operator()(const BootParameters::DFF& dff) const
    {
      NOTICE_LOG(BOOT, "Booting DFF: %s", dff.dff_path.c_str());
      return FifoPlayer::GetInstance().Open(dff.dff_path);
    }

  private:
    const SConfig& config;
  };

  if (!std::visit(BootTitle(), boot->parameters))
    return false;

  std::string game_id = SConfig::GetInstance().m_debugger_game_id;

  if (ARBruteForcer::ch_bruteforce)
	  ARBruteForcer::ParseMapFile(game_id);

  //if (game_id.size() >= 4)
  //  VideoInterface::SetRegionReg(game_id.at(3));
  PatchEngine::LoadPatches();
  HLE::PatchFixedFunctions();
  HideObjectEngine::LoadHideObjects();
  HideObjectEngine::ApplyFrameHideObjects();
  return true;
}

BootExecutableReader::BootExecutableReader(const std::string& file_name)
    : BootExecutableReader(File::IOFile{file_name, "rb"})
{
}

BootExecutableReader::BootExecutableReader(File::IOFile file)
{
  file.Seek(0, SEEK_SET);
  m_bytes.resize(file.GetSize());
  file.ReadBytes(m_bytes.data(), m_bytes.size());
}

BootExecutableReader::BootExecutableReader(const std::vector<u8>& bytes) : m_bytes(bytes)
{
}

BootExecutableReader::~BootExecutableReader() = default;

void StateFlags::UpdateChecksum()
{
  constexpr size_t length_in_bytes = sizeof(StateFlags) - 4;
  constexpr size_t num_elements = length_in_bytes / sizeof(u32);
  std::array<u32, num_elements> flag_data;
  std::memcpy(flag_data.data(), &flags, length_in_bytes);
  checksum = std::accumulate(flag_data.cbegin(), flag_data.cend(), 0U);
}

void UpdateStateFlags(std::function<void(StateFlags*)> update_function)
{
  const std::string file_path =
      Common::GetTitleDataPath(Titles::SYSTEM_MENU, Common::FROM_SESSION_ROOT) + WII_STATE;

  File::IOFile file;
  StateFlags state;
  if (File::Exists(file_path))
  {
    file.Open(file_path, "r+b");
    file.ReadBytes(&state, sizeof(state));
  }
  else
  {
    File::CreateFullPath(file_path);
    file.Open(file_path, "a+b");
    memset(&state, 0, sizeof(state));
  }

  update_function(&state);
  state.UpdateChecksum();

  file.Seek(0, SEEK_SET);
  file.WriteBytes(&state, sizeof(state));
}
