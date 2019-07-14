// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/Boot/Boot.h"

#ifdef _MSC_VER
#include <filesystem>
namespace fs = std::filesystem;
#define HAS_STD_FILESYSTEM
#endif

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <numeric>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
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

#include "Core/Boot/DolReader.h"
#include "Core/Boot/ElfReader.h"
#include "Core/CommonTitles.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/ConfigManager.h"
#include "Core/FifoPlayer/FifoPlayer.h"
#include "Core/HLE/HLE.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/EXI/EXI_DeviceIPL.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/VideoInterface.h"
#include "Core/Host.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/FS/FileSystem.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/IOSC.h"
#include "Core/IOS/Uids.h"
#include "Core/PatchEngine.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"

#include "DiscIO/Enums.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeWad.h"

static std::vector<std::string> ReadM3UFile(const std::string& m3u_path,
                                            const std::string& folder_path)
{
#ifndef HAS_STD_FILESYSTEM
  ASSERT(folder_path.back() == '/');
#endif

  std::vector<std::string> result;
  std::vector<std::string> nonexistent;

  std::ifstream s;
  File::OpenFStream(s, m3u_path, std::ios_base::in);

  std::string line;
  while (std::getline(s, line))
  {
    // This is the UTF-8 representation of U+FEFF.
    const std::string utf8_bom = "\xEF\xBB\xBF";

    if (StringBeginsWith(line, utf8_bom))
    {
      WARN_LOG(BOOT, "UTF-8 BOM in file: %s", m3u_path.c_str());
      line.erase(0, utf8_bom.length());
    }

    if (!line.empty() && line.front() != '#')  // Comments start with #
    {
#ifdef HAS_STD_FILESYSTEM
      const std::string path_to_add = PathToString(StringToPath(folder_path) / StringToPath(line));
#else
      const std::string path_to_add = line.front() != '/' ? folder_path + line : line;
#endif

      (File::Exists(path_to_add) ? result : nonexistent).push_back(path_to_add);
    }
  }

  if (!nonexistent.empty())
  {
    PanicAlertT("Files specified in the M3U file \"%s\" were not found:\n%s", m3u_path.c_str(),
                JoinStrings(nonexistent, "\n").c_str());
    return {};
  }

  if (result.empty())
    PanicAlertT("No paths found in the M3U file \"%s\"", m3u_path.c_str());

  return result;
}

BootParameters::BootParameters(Parameters&& parameters_,
                               const std::optional<std::string>& savestate_path_)
    : parameters(std::move(parameters_)), savestate_path(savestate_path_)
{
}

std::unique_ptr<BootParameters>
BootParameters::GenerateFromFile(std::string boot_path,
                                 const std::optional<std::string>& savestate_path)
{
  return GenerateFromFile(std::vector<std::string>{std::move(boot_path)}, savestate_path);
}

std::unique_ptr<BootParameters>
BootParameters::GenerateFromFile(std::vector<std::string> paths,
                                 const std::optional<std::string>& savestate_path)
{
  ASSERT(!paths.empty());

  const bool is_drive = Common::IsCDROMDevice(paths.front());
  // Check if the file exist, we may have gotten it from a --elf command line
  // that gave an incorrect file name
  if (!is_drive && !File::Exists(paths.front()))
  {
    PanicAlertT("The specified file \"%s\" does not exist", paths.front().c_str());
    return {};
  }

  std::string folder_path;
  std::string extension;
  SplitPath(paths.front(), &folder_path, nullptr, &extension);
  std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

  if (extension == ".m3u" || extension == ".m3u8")
  {
    paths = ReadM3UFile(paths.front(), folder_path);
    if (paths.empty())
      return {};

    SplitPath(paths.front(), nullptr, nullptr, &extension);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
  }

  std::string path = paths.front();
  if (paths.size() == 1)
    paths.clear();

  static const std::unordered_set<std::string> disc_image_extensions = {
      {".gcm", ".iso", ".tgc", ".wbfs", ".ciso", ".gcz", ".dol", ".elf"}};
  if (disc_image_extensions.find(extension) != disc_image_extensions.end() || is_drive)
  {
    std::unique_ptr<DiscIO::VolumeDisc> disc = DiscIO::CreateDisc(path);
    if (disc)
    {
      return std::make_unique<BootParameters>(Disc{std::move(path), std::move(disc), paths},
                                              savestate_path);
    }

    if (extension == ".elf")
    {
      auto elf_reader = std::make_unique<ElfReader>(path);
      return std::make_unique<BootParameters>(Executable{std::move(path), std::move(elf_reader)},
                                              savestate_path);
    }

    if (extension == ".dol")
    {
      auto dol_reader = std::make_unique<DolReader>(path);
      return std::make_unique<BootParameters>(Executable{std::move(path), std::move(dol_reader)},
                                              savestate_path);
    }

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
    return std::make_unique<BootParameters>(DFF{std::move(path)}, savestate_path);

  if (extension == ".wad")
  {
    std::unique_ptr<DiscIO::VolumeWAD> wad = DiscIO::CreateWAD(std::move(path));
    if (wad)
      return std::make_unique<BootParameters>(std::move(*wad), savestate_path);
  }

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
static const DiscIO::VolumeDisc* SetDisc(std::unique_ptr<DiscIO::VolumeDisc> disc,
                                         std::vector<std::string> auto_disc_change_paths = {})
{
  const DiscIO::VolumeDisc* pointer = disc.get();
  DVDInterface::SetDisc(std::move(disc), auto_disc_change_paths);
  return pointer;
}

bool CBoot::DVDRead(const DiscIO::VolumeDisc& disc, u64 dvd_offset, u32 output_address, u32 length,
                    const DiscIO::Partition& partition)
{
  std::vector<u8> buffer(length);
  if (!disc.Read(dvd_offset, length, buffer.data(), partition))
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

  static const std::array<std::string, 2> maps_directories{
      File::GetUserPath(D_MAPS_IDX),
      File::GetSysDirectory() + MAPS_DIR DIR_SEP,
  };
  for (const auto& directory : maps_directories)
  {
    std::string path = directory + game_id + ".map";
    if (File::Exists(path))
    {
      if (existing_map_file)
        *existing_map_file = std::move(path);

      return true;
    }
  }

  return false;
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
  // CRC32 hashes of the IPL file, obtained from Redump
  constexpr u32 NTSC_v1_0 = 0x6DAC1F2A;
  constexpr u32 NTSC_v1_1 = 0xD5E6FEEA;
  constexpr u32 NTSC_v1_2 = 0x86573808;
  constexpr u32 MPAL_v1_1 = 0x667D0B64;  // Brazil
  constexpr u32 PAL_v1_0 = 0x4F319F43;
  constexpr u32 PAL_v1_2 = 0xAD1B7F16;

  // Load the whole ROM dump
  std::string data;
  if (!File::ReadFileToString(boot_rom_filename, data))
    return false;

  // Use zlibs crc32 implementation to compute the hash
  u32 ipl_hash = crc32(0L, Z_NULL, 0);
  ipl_hash = crc32(ipl_hash, (const Bytef*)data.data(), (u32)data.size());
  bool known_ipl = false;
  bool pal_ipl = false;
  switch (ipl_hash)
  {
  case NTSC_v1_0:
  case NTSC_v1_1:
  case NTSC_v1_2:
  case MPAL_v1_1:
    known_ipl = true;
    break;
  case PAL_v1_0:
  case PAL_v1_2:
    pal_ipl = true;
    known_ipl = true;
    break;
  default:
    PanicAlertT("The IPL file is not a known good dump. (CRC32: %x)", ipl_hash);
    break;
  }

  const DiscIO::Region boot_region = SConfig::GetInstance().m_region;
  if (known_ipl && pal_ipl != (boot_region == DiscIO::Region::PAL))
  {
    PanicAlertT("%s IPL found in %s directory. The disc might not be recognized",
                pal_ipl ? "PAL" : "NTSC", SConfig::GetDirectoryForRegion(boot_region));
  }

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

  MSR.FP = 1;
  MSR.DR = 1;
  MSR.IR = 1;

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
  const std::string default_iso = Config::Get(Config::MAIN_DEFAULT_ISO);
  if (!default_iso.empty())
    SetDisc(DiscIO::CreateDisc(default_iso));
}

static void CopyDefaultExceptionHandlers()
{
  constexpr u32 EXCEPTION_HANDLER_ADDRESSES[] = {0x00000100, 0x00000200, 0x00000300, 0x00000400,
                                                 0x00000500, 0x00000600, 0x00000700, 0x00000800,
                                                 0x00000900, 0x00000C00, 0x00000D00, 0x00000F00,
                                                 0x00001300, 0x00001400, 0x00001700};

  constexpr u32 RFI_INSTRUCTION = 0x4C000064;
  for (const u32 address : EXCEPTION_HANDLER_ADDRESSES)
    Memory::Write_U32(RFI_INSTRUCTION, address);
}

// Third boot step after BootManager and Core. See Call schedule in BootManager.cpp
bool CBoot::BootUp(std::unique_ptr<BootParameters> boot)
{
  SConfig& config = SConfig::GetInstance();

  if (!g_symbolDB.IsEmpty())
  {
    g_symbolDB.Clear();
    UpdateDebugger_MapLoaded();
  }

  // PAL Wii uses NTSC framerate and linecount in 60Hz modes
  VideoInterface::Preset(DiscIO::IsNTSC(config.m_region) ||
                         (config.bWii && Config::Get(Config::SYSCONF_PAL60)));

  struct BootTitle
  {
    BootTitle() : config(SConfig::GetInstance()) {}
    bool operator()(BootParameters::Disc& disc) const
    {
      NOTICE_LOG(BOOT, "Booting from disc: %s", disc.path.c_str());
      const DiscIO::VolumeDisc* volume =
          SetDisc(std::move(disc.volume), disc.auto_disc_change_paths);

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
      CopyDefaultExceptionHandlers();

      if (config.bWii)
      {
        PowerPC::ppcState.spr[SPR_HID0] = 0x0011c464;
        PowerPC::ppcState.spr[SPR_HID4] = 0x82000000;

        // Set a value for the SP. It doesn't matter where this points to,
        // as long as it is a valid location. This value is taken from a homebrew binary.
        PowerPC::ppcState.gpr[1] = 0x8004d4bc;

        // Because there is no TMD to get the requested system (IOS) version from,
        // we default to IOS58, which is the version used by the Homebrew Channel.
        SetupWiiMemory(IOS::HLE::IOSC::ConsoleType::Retail);
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

    bool operator()(const DiscIO::VolumeWAD& wad) const
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
        SetDisc(DiscIO::CreateDisc(ipl.disc->path), ipl.disc->auto_disc_change_paths);
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

  PatchEngine::LoadPatches();
  HLE::PatchFixedFunctions();
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

BootExecutableReader::BootExecutableReader(std::vector<u8> bytes) : m_bytes(std::move(bytes))
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
  CreateSystemMenuTitleDirs();
  const std::string file_path = Common::GetTitleDataPath(Titles::SYSTEM_MENU) + "/" WII_STATE;
  const auto fs = IOS::HLE::GetIOS()->GetFS();
  constexpr IOS::HLE::FS::Mode rw_mode = IOS::HLE::FS::Mode::ReadWrite;
  const auto file = fs->CreateAndOpenFile(IOS::SYSMENU_UID, IOS::SYSMENU_GID, file_path,
                                          {rw_mode, rw_mode, rw_mode});
  if (!file)
    return;

  StateFlags state{};
  if (file->GetStatus()->size == sizeof(StateFlags))
    file->Read(&state, 1);

  update_function(&state);
  state.UpdateChecksum();

  file->Seek(0, IOS::HLE::FS::SeekMode::Set);
  file->Write(&state, 1);
}

void CreateSystemMenuTitleDirs()
{
  const auto es = IOS::HLE::GetIOS()->GetES();
  es->CreateTitleDirectories(Titles::SYSTEM_MENU, IOS::SYSMENU_GID);
}
