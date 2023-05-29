// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/NandPaths.h"
#include "Common/SettingsHandler.h"

#include "Core/Boot/Boot.h"
#include "Core/CommonTitles.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HLE/HLE.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/EXI/EXI_DeviceIPL.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/DI/DI.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/IOS/FS/FileSystem.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/IOSC.h"
#include "Core/IOS/Uids.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

#include "DiscIO/Enums.h"
#include "DiscIO/RiivolutionPatcher.h"
#include "DiscIO/VolumeDisc.h"

#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/XFMemory.h"

namespace
{
void PresetTimeBaseTicks(Core::System& system, const Core::CPUThreadGuard& guard)
{
  const u64 emulated_time =
      ExpansionInterface::CEXIIPL::GetEmulatedTime(system, ExpansionInterface::CEXIIPL::GC_EPOCH);

  const u64 time_base_ticks = emulated_time * 40500000ULL;

  PowerPC::MMU::HostWrite_U64(guard, time_base_ticks, 0x800030D8);
}
}  // Anonymous namespace

void CBoot::RunFunction(Core::System& system, u32 address)
{
  auto& power_pc = system.GetPowerPC();
  auto& ppc_state = power_pc.GetPPCState();

  ppc_state.pc = address;
  LR(ppc_state) = 0x00;

  while (ppc_state.pc != 0x00)
    power_pc.SingleStep();
}

void CBoot::SetupMSR(PowerPC::PowerPCState& ppc_state)
{
  // 0x0002032
  ppc_state.msr.RI = 1;
  ppc_state.msr.DR = 1;
  ppc_state.msr.IR = 1;
  ppc_state.msr.FP = 1;
}

void CBoot::SetupHID(PowerPC::PowerPCState& ppc_state, bool is_wii)
{
  // HID0 is 0x0011c464 on GC, 0x0011c664 on Wii
  HID0(ppc_state).BHT = 1;
  HID0(ppc_state).BTIC = 1;
  HID0(ppc_state).DCFA = 1;
  if (is_wii)
    HID0(ppc_state).SPD = 1;
  HID0(ppc_state).DCFI = 1;
  HID0(ppc_state).DCE = 1;
  // Note that Datel titles will fail to boot if the instruction cache is not enabled; see
  // https://bugs.dolphin-emu.org/issues/8223
  HID0(ppc_state).ICE = 1;
  HID0(ppc_state).NHR = 1;
  HID0(ppc_state).DPM = 1;

  // HID1 is initialized in PowerPC.cpp to 0x80000000
  // HID2 is 0xe0000000
  HID2(ppc_state).PSE = 1;
  HID2(ppc_state).WPE = 1;
  HID2(ppc_state).LSQE = 1;

  // HID4 is 0 on GC and 0x83900000 on Wii
  if (is_wii)
  {
    HID4(ppc_state).L2CFI = 1;
    HID4(ppc_state).LPE = 1;
    HID4(ppc_state).ST0 = 1;
    HID4(ppc_state).SBE = 1;
    HID4(ppc_state).reserved_3 = 1;
  }
}

void CBoot::SetupBAT(Core::System& system, bool is_wii)
{
  auto& ppc_state = system.GetPPCState();
  ppc_state.spr[SPR_IBAT0U] = 0x80001fff;
  ppc_state.spr[SPR_IBAT0L] = 0x00000002;
  ppc_state.spr[SPR_DBAT0U] = 0x80001fff;
  ppc_state.spr[SPR_DBAT0L] = 0x00000002;
  ppc_state.spr[SPR_DBAT1U] = 0xc0001fff;
  ppc_state.spr[SPR_DBAT1L] = 0x0000002a;
  if (is_wii)
  {
    ppc_state.spr[SPR_IBAT4U] = 0x90001fff;
    ppc_state.spr[SPR_IBAT4L] = 0x10000002;
    ppc_state.spr[SPR_DBAT4U] = 0x90001fff;
    ppc_state.spr[SPR_DBAT4L] = 0x10000002;
    ppc_state.spr[SPR_DBAT5U] = 0xd0001fff;
    ppc_state.spr[SPR_DBAT5L] = 0x1000002a;
    HID4(ppc_state).SBE = 1;
  }

  auto& mmu = system.GetMMU();
  mmu.DBATUpdated();
  mmu.IBATUpdated();
}

bool CBoot::RunApploader(Core::System& system, const Core::CPUThreadGuard& guard, bool is_wii,
                         const DiscIO::VolumeDisc& volume,
                         const std::vector<DiscIO::Riivolution::Patch>& riivolution_patches)
{
  const DiscIO::Partition partition = volume.GetGamePartition();

  // Load Apploader to Memory - The apploader is hardcoded to begin at 0x2440 on the disc,
  // but the size can differ between discs. Compare with YAGCD chap 13.
  constexpr u32 offset = 0x2440;
  const std::optional<u32> entry = volume.ReadSwapped<u32>(offset + 0x10, partition);
  const std::optional<u32> size = volume.ReadSwapped<u32>(offset + 0x14, partition);
  const std::optional<u32> trailer = volume.ReadSwapped<u32>(offset + 0x18, partition);
  if (!entry || !size || !trailer || *entry == UINT32_MAX || *size + *trailer == UINT32_MAX)
  {
    INFO_LOG_FMT(BOOT, "Invalid apploader. Your disc image is probably corrupted.");
    return false;
  }
  DVDRead(system, volume, offset + 0x20, 0x01200000, *size + *trailer, partition);

  // TODO - Make Apploader(or just RunFunction()) debuggable!!!

  auto& ppc_state = system.GetPPCState();
  auto& mmu = system.GetMMU();

  // Call iAppLoaderEntry.
  DEBUG_LOG_FMT(BOOT, "Call iAppLoaderEntry");
  const u32 iAppLoaderFuncAddr = is_wii ? 0x80004000 : 0x80003100;
  ppc_state.gpr[3] = iAppLoaderFuncAddr + 0;
  ppc_state.gpr[4] = iAppLoaderFuncAddr + 4;
  ppc_state.gpr[5] = iAppLoaderFuncAddr + 8;
  RunFunction(system, *entry);
  const u32 iAppLoaderInit = mmu.Read_U32(iAppLoaderFuncAddr + 0);
  const u32 iAppLoaderMain = mmu.Read_U32(iAppLoaderFuncAddr + 4);
  const u32 iAppLoaderClose = mmu.Read_U32(iAppLoaderFuncAddr + 8);

  // iAppLoaderInit
  DEBUG_LOG_FMT(BOOT, "Call iAppLoaderInit");
  PowerPC::MMU::HostWrite_U32(guard, 0x4E800020, 0x81300000);  // Write BLR
  HLE::Patch(system, 0x81300000, "AppLoaderReport");           // HLE OSReport for Apploader
  ppc_state.gpr[3] = 0x81300000;
  RunFunction(system, iAppLoaderInit);

  // iAppLoaderMain - Here we load the apploader, the DOL (the exe) and the FST (filesystem).
  // To give you an idea about where the stuff is located on the disc take a look at yagcd
  // ch 13.
  DEBUG_LOG_FMT(BOOT, "Call iAppLoaderMain");

  ppc_state.gpr[3] = 0x81300004;
  ppc_state.gpr[4] = 0x81300008;
  ppc_state.gpr[5] = 0x8130000c;

  RunFunction(system, iAppLoaderMain);

  // iAppLoaderMain returns 1 if the pointers in R3/R4/R5 were filled with values for DVD copy
  // Typical behaviour is doing it once for each section defined in the DOL header. Some unlicensed
  // titles don't have a properly constructed DOL and maintain a table of these values in apploader.
  // iAppLoaderMain returns 0 when there are no more sections to copy.
  while (ppc_state.gpr[3] != 0x00)
  {
    const u32 ram_address = mmu.Read_U32(0x81300004);
    const u32 length = mmu.Read_U32(0x81300008);
    const u32 dvd_offset = mmu.Read_U32(0x8130000c) << (is_wii ? 2 : 0);

    INFO_LOG_FMT(BOOT, "DVDRead: offset: {:08x}   memOffset: {:08x}   length: {}", dvd_offset,
                 ram_address, length);
    DVDRead(system, volume, dvd_offset, ram_address, length, partition);

    DiscIO::Riivolution::ApplyApploaderMemoryPatches(guard, riivolution_patches, ram_address,
                                                     length);

    ppc_state.gpr[3] = 0x81300004;
    ppc_state.gpr[4] = 0x81300008;
    ppc_state.gpr[5] = 0x8130000c;

    RunFunction(system, iAppLoaderMain);
  }

  // iAppLoaderClose
  DEBUG_LOG_FMT(BOOT, "call iAppLoaderClose");
  RunFunction(system, iAppLoaderClose);
  HLE::UnPatch(system, "AppLoaderReport");

  // return
  ppc_state.pc = ppc_state.gpr[3];

  return true;
}

void CBoot::SetupGCMemory(Core::System& system, const Core::CPUThreadGuard& guard)
{
  auto& memory = system.GetMemory();

  // Booted from bootrom. 0xE5207C22 = booted from jtag
  PowerPC::MMU::HostWrite_U32(guard, 0x0D15EA5E, 0x80000020);

  // Physical Memory Size (24MB on retail)
  PowerPC::MMU::HostWrite_U32(guard, memory.GetRamSizeReal(), 0x80000028);

  // Console type - DevKit  (retail ID == 0x00000003) see YAGCD 4.2.1.1.2
  // TODO: determine why some games fail when using a retail ID.
  // (Seem to take different EXI paths, see Ikaruga for example)
  const u32 console_type = static_cast<u32>(Core::ConsoleType::LatestDevkit);
  PowerPC::MMU::HostWrite_U32(guard, console_type, 0x8000002C);

  // Fake the VI Init of the IPL (YAGCD 4.2.1.4)
  PowerPC::MMU::HostWrite_U32(guard, DiscIO::IsNTSC(SConfig::GetInstance().m_region) ? 0 : 1,
                              0x800000CC);

  // ARAM Size. 16MB main + 4/16/32MB external. (retail consoles have no external ARAM)
  PowerPC::MMU::HostWrite_U32(guard, 0x01000000, 0x800000d0);

  PowerPC::MMU::HostWrite_U32(guard, 0x09a7ec80, 0x800000F8);  // Bus Clock Speed
  PowerPC::MMU::HostWrite_U32(guard, 0x1cf7c580, 0x800000FC);  // CPU Clock Speed

  PowerPC::MMU::HostWrite_U32(guard, 0x4c000064, 0x80000300);  // Write default DSI Handler:     rfi
  PowerPC::MMU::HostWrite_U32(guard, 0x4c000064, 0x80000800);  // Write default FPU Handler:     rfi
  PowerPC::MMU::HostWrite_U32(guard, 0x4c000064, 0x80000C00);  // Write default Syscall Handler: rfi

  PresetTimeBaseTicks(system, guard);

  // HIO checks this
  // PowerPC::MMU::HostWrite_U16(0x8200, 0x000030e6);   // Console type
}

// __________________________________________________________________________________________________
// GameCube Bootstrap 2 HLE:
// copy the apploader to 0x81200000
// execute the apploader, function by function, using the above utility.
bool CBoot::EmulatedBS2_GC(Core::System& system, const Core::CPUThreadGuard& guard,
                           const DiscIO::VolumeDisc& volume,
                           const std::vector<DiscIO::Riivolution::Patch>& riivolution_patches)
{
  INFO_LOG_FMT(BOOT, "Faking GC BS2...");

  auto& ppc_state = system.GetPPCState();

  SetupMSR(ppc_state);
  SetupHID(ppc_state, /*is_wii*/ false);
  SetupBAT(system, /*is_wii*/ false);

  SetupGCMemory(system, guard);

  // Datel titles don't initialize the postMatrices, but they have dual-texture coordinate
  // transformation enabled. We initialize all of xfmem to 0, which results in everything using
  // a texture coordinate of (0, 0), breaking textures. Normally the IPL will initialize the last
  // entry to the identity matrix, but the whole point of BS2 EMU is that it skips the IPL, so we
  // need to do this initialization ourselves.
  xfmem.postMatrices[0x3d * 4 + 0] = 1.0f;
  xfmem.postMatrices[0x3e * 4 + 1] = 1.0f;
  xfmem.postMatrices[0x3f * 4 + 2] = 1.0f;
  g_vertex_manager->Flush();
  auto& vertex_shader_manager = system.GetVertexShaderManager();
  vertex_shader_manager.InvalidateXFRange(XFMEM_POSTMATRICES + 0x3d * 4, XFMEM_POSTMATRICES_END);

  DVDReadDiscID(system, volume, 0x00000000);

  auto& memory = system.GetMemory();
  bool streaming = memory.Read_U8(0x80000008);
  if (streaming)
  {
    u8 streaming_size = memory.Read_U8(0x80000009);
    // If the streaming buffer size is 0, then BS2 uses a default size of 10 instead.
    // No known game uses a size other than the default.
    if (streaming_size == 0)
      streaming_size = 10;
    system.GetDVDInterface().AudioBufferConfig(true, streaming_size);
  }
  else
  {
    system.GetDVDInterface().AudioBufferConfig(false, 0);
  }

  const bool ntsc = DiscIO::IsNTSC(SConfig::GetInstance().m_region);

  // Setup pointers like real BS2 does

  // StackPointer, used to be set to 0x816ffff0
  ppc_state.gpr[1] = ntsc ? 0x81566550 : 0x815edca8;
  // Global pointer to Small Data Area 2 Base (haven't seen anything use it...meh)
  ppc_state.gpr[2] = ntsc ? 0x81465cc0 : 0x814b5b20;
  // Global pointer to Small Data Area Base (Luigi's Mansion's apploader uses it)
  ppc_state.gpr[13] = ntsc ? 0x81465320 : 0x814b4fc0;

  return RunApploader(system, guard, /*is_wii*/ false, volume, riivolution_patches);
}

static DiscIO::Region CodeRegion(char c)
{
  switch (c)
  {
  case 'J':  // Japan
  case 'T':  // Taiwan
    return DiscIO::Region::NTSC_J;
  case 'B':  // Brazil
  case 'M':  // Middle East
  case 'R':  // Argentina
  case 'S':  // ???
  case 'U':  // USA
  case 'W':  // ???
    return DiscIO::Region::NTSC_U;
  case 'A':  // Australia
  case 'E':  // Europe
    return DiscIO::Region::PAL;
  case 'K':  // Korea
    return DiscIO::Region::NTSC_K;
  default:
    return DiscIO::Region::Unknown;
  }
}

bool CBoot::SetupWiiMemory(Core::System& system, IOS::HLE::IOSC::ConsoleType console_type)
{
  static const std::map<DiscIO::Region, const RegionSetting> region_settings = {
      {DiscIO::Region::NTSC_J, {"JPN", "NTSC", "JP", "LJH"}},
      {DiscIO::Region::NTSC_U, {"USA", "NTSC", "US", "LU"}},
      {DiscIO::Region::PAL, {"EUR", "PAL", "EU", "LEH"}},
      {DiscIO::Region::NTSC_K, {"KOR", "NTSC", "KR", "LKH"}}};
  auto entryPos = region_settings.find(SConfig::GetInstance().m_region);
  RegionSetting region_setting = entryPos->second;

  Common::SettingsHandler gen;
  std::string serno;
  std::string model = "RVL-001(" + region_setting.area + ")";
  CreateSystemMenuTitleDirs();
  const std::string settings_file_path(Common::GetTitleDataPath(Titles::SYSTEM_MENU) +
                                       "/" WII_SETTING);

  const auto fs = IOS::HLE::GetIOS()->GetFS();
  {
    Common::SettingsHandler::Buffer data;
    const auto file = fs->OpenFile(IOS::SYSMENU_UID, IOS::SYSMENU_GID, settings_file_path,
                                   IOS::HLE::FS::Mode::Read);
    if (file && file->Read(data.data(), data.size()))
    {
      gen.SetBytes(std::move(data));
      serno = gen.GetValue("SERNO");
      model = gen.GetValue("MODEL");

      bool region_matches = false;
      if (Config::Get(Config::MAIN_OVERRIDE_REGION_SETTINGS))
      {
        region_matches = true;
      }
      else
      {
        const std::string code = gen.GetValue("CODE");
        if (code.size() >= 2 && CodeRegion(code[1]) == SConfig::GetInstance().m_region)
          region_matches = true;
      }

      if (region_matches)
      {
        region_setting = RegionSetting{gen.GetValue("AREA"), gen.GetValue("VIDEO"),
                                       gen.GetValue("GAME"), gen.GetValue("CODE")};
      }
      else
      {
        const size_t parenthesis_pos = model.find('(');
        if (parenthesis_pos != std::string::npos)
          model = model.substr(0, parenthesis_pos) + '(' + region_setting.area + ')';
      }

      gen.Reset();
    }
  }
  fs->Delete(IOS::SYSMENU_UID, IOS::SYSMENU_GID, settings_file_path);

  if (serno.empty() || serno == "000000000")
  {
    if (Core::WantsDeterminism())
      serno = "123456789";
    else
      serno = Common::SettingsHandler::GenerateSerialNumber();
    INFO_LOG_FMT(BOOT, "No previous serial number found, generated one instead: {}", serno);
  }
  else
  {
    INFO_LOG_FMT(BOOT, "Using serial number: {}", serno);
  }

  gen.AddSetting("AREA", region_setting.area);
  gen.AddSetting("MODEL", model);
  gen.AddSetting("DVD", "0");
  gen.AddSetting("MPCH", "0x7FFE");
  gen.AddSetting("CODE", region_setting.code);
  gen.AddSetting("SERNO", serno);
  gen.AddSetting("VIDEO", region_setting.video);
  gen.AddSetting("GAME", region_setting.game);

  constexpr IOS::HLE::FS::Mode rw_mode = IOS::HLE::FS::Mode::ReadWrite;
  const auto settings_file = fs->CreateAndOpenFile(IOS::SYSMENU_UID, IOS::SYSMENU_GID,
                                                   settings_file_path, {rw_mode, rw_mode, rw_mode});
  if (!settings_file || !settings_file->Write(gen.GetBytes().data(), gen.GetBytes().size()))
  {
    PanicAlertFmtT("SetupWiiMemory: Can't create setting.txt file");
    return false;
  }

  auto& memory = system.GetMemory();

  // Write the 256 byte setting.txt to memory.
  memory.CopyToEmu(0x3800, gen.GetBytes().data(), gen.GetBytes().size());

  INFO_LOG_FMT(BOOT, "Setup Wii Memory...");

  /*
  Set hardcoded global variables to Wii memory. These are partly collected from
  WiiBrew. These values are needed for the games to function correctly. A few
  values in this region will also be placed here by the game as it boots.
  They are:
  0x80000038  Start of FST
  0x8000003c  Size of FST Size
  0x80000060  Copyright code
  */

  memory.Write_U32(0x0D15EA5E, 0x00000020);               // Another magic word
  memory.Write_U32(0x00000001, 0x00000024);               // Unknown
  memory.Write_U32(memory.GetRamSizeReal(), 0x00000028);  // MEM1 size 24MB
  const Core::ConsoleType board_model = console_type == IOS::HLE::IOSC::ConsoleType::RVT ?
                                            Core::ConsoleType::NDEV2_1 :
                                            Core::ConsoleType::RVL_Retail3;
  memory.Write_U32(static_cast<u32>(board_model), 0x0000002c);  // Board Model
  memory.Write_U32(0x00000000, 0x00000030);                     // Init
  memory.Write_U32(0x817FEC60, 0x00000034);                     // Init
  // 38, 3C should get start, size of FST through apploader
  memory.Write_U32(0x8008f7b8, 0x000000e4);               // Thread Init
  memory.Write_U32(memory.GetRamSizeReal(), 0x000000f0);  // "Simulated memory size" (debug mode?)
  memory.Write_U32(0x8179b500, 0x000000f4);               // __start
  memory.Write_U32(0x0e7be2c0, 0x000000f8);               // Bus speed
  memory.Write_U32(0x2B73A840, 0x000000fc);               // CPU speed
  memory.Write_U16(0x0000, 0x000030e6);                   // Console type
  memory.Write_U32(0x00000000, 0x000030c0);               // EXI
  memory.Write_U32(0x00000000, 0x000030c4);               // EXI
  memory.Write_U32(0x00000000, 0x000030dc);               // Time
  memory.Write_U32(0xffffffff, 0x000030d8);               // Unknown, set by any official NAND title
  memory.Write_U16(0x8201, 0x000030e6);                   // Dev console / debug capable
  memory.Write_U32(0x00000000, 0x000030f0);               // Apploader

  // During the boot process, 0x315c is first set to 0xdeadbeef by IOS
  // in the boot_ppc syscall. The value is then partly overwritten by SDK titles.
  // Two bytes at 0x315e are used to indicate the "devkit boot program version".
  // It is only written to by the system menu, so we must do so here as well.
  //
  // 0x0113 appears to mean v1.13, which is the latest version.
  // It is fine to always use the latest value as apploaders work with all versions.
  memory.Write_U16(0x0113, 0x0000315e);

  memory.Write_U8(0x80, 0x0000315c);         // OSInit
  memory.Write_U16(0x0000, 0x000030e0);      // PADInit
  memory.Write_U32(0x80000000, 0x00003184);  // GameID Address

  // Fake the VI Init of the IPL
  memory.Write_U32(DiscIO::IsNTSC(SConfig::GetInstance().m_region) ? 0 : 1, 0x000000CC);

  // Clear exception handler. Why? Don't we begin with only zeros?
  for (int i = 0x3000; i <= 0x3038; i += 4)
  {
    memory.Write_U32(0x00000000, i);
  }

  return true;
}

static void WriteEmptyPlayRecord()
{
  CreateSystemMenuTitleDirs();
  const std::string file_path = Common::GetTitleDataPath(Titles::SYSTEM_MENU) + "/play_rec.dat";
  const auto fs = IOS::HLE::GetIOS()->GetFS();
  constexpr IOS::HLE::FS::Mode rw_mode = IOS::HLE::FS::Mode::ReadWrite;
  const auto playrec_file = fs->CreateAndOpenFile(IOS::SYSMENU_UID, IOS::SYSMENU_GID, file_path,
                                                  {rw_mode, rw_mode, rw_mode});
  if (!playrec_file)
    return;
  std::vector<u8> empty_record(0x80);
  playrec_file->Write(empty_record.data(), empty_record.size());
}

// __________________________________________________________________________________________________
// Wii Bootstrap 2 HLE:
// copy the apploader to 0x81200000
// execute the apploader
bool CBoot::EmulatedBS2_Wii(Core::System& system, const Core::CPUThreadGuard& guard,
                            const DiscIO::VolumeDisc& volume,
                            const std::vector<DiscIO::Riivolution::Patch>& riivolution_patches)
{
  INFO_LOG_FMT(BOOT, "Faking Wii BS2...");
  if (volume.GetVolumeType() != DiscIO::Platform::WiiDisc)
    return false;

  const DiscIO::Partition partition = volume.GetGamePartition();
  const IOS::ES::TMDReader tmd = volume.GetTMD(partition);

  if (!tmd.IsValid())
    return false;

  WriteEmptyPlayRecord();
  UpdateStateFlags([](StateFlags* state) {
    state->flags = 0xc1;
    state->type = 0xff;
    state->discstate = 0x01;
  });

  auto& memory = system.GetMemory();

  // The system menu clears the RTC flags.
  // However, the system menu also updates the disc cache when this happens; see
  // https://wiibrew.org/wiki/MX23L4005#DI and
  // https://wiibrew.org/wiki//title/00000001/00000002/data/cache.dat for details. If we clear the
  // RTC flags, then the system menu thinks the disc cache is up to date, and will show the wrong
  // disc in the disc channel (and reboot the first time the disc is opened)
  // ExpansionInterface::g_rtc_flags.m_hex = 0;

  // While reading a disc, the system menu reads the first partition table
  // (0x20 bytes from 0x00040020) and stores a pointer to the data partition entry.
  // When launching the disc game, it copies the partition type and offset to 0x3194
  // and 0x3198 respectively.
  const DiscIO::Partition data_partition = volume.GetGamePartition();
  memory.Write_U32(0, 0x3194);
  memory.Write_U32(static_cast<u32>(data_partition.offset >> 2), 0x3198);

  const s32 ios_override = Config::Get(Config::MAIN_OVERRIDE_BOOT_IOS);
  const u64 ios = ios_override >= 0 ? Titles::IOS(static_cast<u32>(ios_override)) : tmd.GetIOSId();

  const auto console_type = volume.GetTicket(data_partition).GetConsoleType();
  if (!SetupWiiMemory(system, console_type) || !IOS::HLE::GetIOS()->BootIOS(system, ios))
    return false;

  auto di =
      std::static_pointer_cast<IOS::HLE::DIDevice>(IOS::HLE::GetIOS()->GetDeviceByName("/dev/di"));

  di->InitializeIfFirstTime();
  di->ChangePartition(data_partition);

  DVDReadDiscID(system, volume, 0x00000000);

  // This is some kind of consistency check that is compared to the 0x00
  // values as the game boots. This location keeps the 4 byte ID for as long
  // as the game is running. The 6 byte ID at 0x00 is overwritten sometime
  // after this check during booting.
  DVDRead(system, volume, 0, 0x3180, 4, partition);

  auto& ppc_state = system.GetPPCState();

  SetupMSR(ppc_state);
  SetupHID(ppc_state, /*is_wii*/ true);
  SetupBAT(system, /*is_wii*/ true);

  memory.Write_U32(0x4c000064, 0x00000300);  // Write default DSI Handler:   rfi
  memory.Write_U32(0x4c000064, 0x00000800);  // Write default FPU Handler:   rfi
  memory.Write_U32(0x4c000064, 0x00000C00);  // Write default Syscall Handler: rfi

  ppc_state.gpr[1] = 0x816ffff0;  // StackPointer

  if (!RunApploader(system, guard, /*is_wii*/ true, volume, riivolution_patches))
    return false;

  // The Apploader probably just overwrote values needed for RAM Override.  Run this again!
  IOS::HLE::RAMOverrideForIOSMemoryValues(IOS::HLE::MemorySetupType::IOSReload);

  // Warning: This call will set incorrect running game metadata if our volume parameter
  // doesn't point to the same disc as the one that's inserted in the emulated disc drive!
  IOS::HLE::GetIOS()->GetESDevice()->DIVerify(tmd, volume.GetTicket(partition));

  return true;
}

// Returns true if apploader has run successfully. If is_wii is true, the disc
// that volume refers to must currently be inserted into the emulated disc drive.
bool CBoot::EmulatedBS2(Core::System& system, const Core::CPUThreadGuard& guard, bool is_wii,
                        const DiscIO::VolumeDisc& volume,
                        const std::vector<DiscIO::Riivolution::Patch>& riivolution_patches)
{
  return is_wii ? EmulatedBS2_Wii(system, guard, volume, riivolution_patches) :
                  EmulatedBS2_GC(system, guard, volume, riivolution_patches);
}
