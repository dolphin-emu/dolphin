// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/NandPaths.h"
#include "Common/SettingsHandler.h"

#include "Core/Boot/Boot.h"
#include "Core/CommonTitles.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HLE/HLE.h"
#include "Core/HW/EXI/EXI_DeviceIPL.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/IOS/IOS.h"
#include "Core/PowerPC/PowerPC.h"

#include "DiscIO/Enums.h"
#include "DiscIO/Volume.h"

namespace
{
void PresetTimeBaseTicks()
{
  const u64 emulated_time =
      ExpansionInterface::CEXIIPL::GetEmulatedTime(ExpansionInterface::CEXIIPL::GC_EPOCH);

  const u64 time_base_ticks = emulated_time * 40500000ULL;

  PowerPC::HostWrite_U64(time_base_ticks, 0x800030D8);
}
}  // Anonymous namespace

void CBoot::RunFunction(u32 address)
{
  PC = address;
  LR = 0x00;

  while (PC != 0x00)
    PowerPC::SingleStep();
}

void CBoot::SetupMSR()
{
  UReg_MSR& m_MSR = ((UReg_MSR&)PowerPC::ppcState.msr);
  m_MSR.FP = 1;
  m_MSR.DR = 1;
  m_MSR.IR = 1;
  m_MSR.EE = 1;
}

void CBoot::SetupBAT(bool is_wii)
{
  PowerPC::ppcState.spr[SPR_IBAT0U] = 0x80001fff;
  PowerPC::ppcState.spr[SPR_IBAT0L] = 0x00000002;
  PowerPC::ppcState.spr[SPR_DBAT0U] = 0x80001fff;
  PowerPC::ppcState.spr[SPR_DBAT0L] = 0x00000002;
  PowerPC::ppcState.spr[SPR_DBAT1U] = 0xc0001fff;
  PowerPC::ppcState.spr[SPR_DBAT1L] = 0x0000002a;
  if (is_wii)
  {
    PowerPC::ppcState.spr[SPR_IBAT4U] = 0x90001fff;
    PowerPC::ppcState.spr[SPR_IBAT4L] = 0x10000002;
    PowerPC::ppcState.spr[SPR_DBAT4U] = 0x90001fff;
    PowerPC::ppcState.spr[SPR_DBAT4L] = 0x10000002;
    PowerPC::ppcState.spr[SPR_DBAT5U] = 0xd0001fff;
    PowerPC::ppcState.spr[SPR_DBAT5L] = 0x1000002a;
  }
  PowerPC::DBATUpdated();
  PowerPC::IBATUpdated();
}

bool CBoot::RunApploader(bool is_wii, const DiscIO::Volume& volume)
{
  const DiscIO::Partition partition = volume.GetGamePartition();

  // Load Apploader to Memory - The apploader is hardcoded to begin at 0x2440 on the disc,
  // but the size can differ between discs. Compare with YAGCD chap 13.
  constexpr u32 offset = 0x2440;
  const std::optional<u32> entry = volume.ReadSwapped<u32>(offset + 0x10, partition);
  const std::optional<u32> size = volume.ReadSwapped<u32>(offset + 0x14, partition);
  const std::optional<u32> trailer = volume.ReadSwapped<u32>(offset + 0x18, partition);
  if (!entry || !size || !trailer || *entry == (u32)-1 || *size + *trailer == (u32)-1)
  {
    INFO_LOG(BOOT, "Invalid apploader. Your disc image is probably corrupted.");
    return false;
  }
  DVDRead(volume, offset + 0x20, 0x01200000, *size + *trailer, partition);

  // TODO - Make Apploader(or just RunFunction()) debuggable!!!

  // Call iAppLoaderEntry.
  DEBUG_LOG(MASTER_LOG, "Call iAppLoaderEntry");
  const u32 iAppLoaderFuncAddr = is_wii ? 0x80004000 : 0x80003100;
  PowerPC::ppcState.gpr[3] = iAppLoaderFuncAddr + 0;
  PowerPC::ppcState.gpr[4] = iAppLoaderFuncAddr + 4;
  PowerPC::ppcState.gpr[5] = iAppLoaderFuncAddr + 8;
  RunFunction(*entry);
  const u32 iAppLoaderInit = PowerPC::Read_U32(iAppLoaderFuncAddr + 0);
  const u32 iAppLoaderMain = PowerPC::Read_U32(iAppLoaderFuncAddr + 4);
  const u32 iAppLoaderClose = PowerPC::Read_U32(iAppLoaderFuncAddr + 8);

  // iAppLoaderInit
  DEBUG_LOG(MASTER_LOG, "Call iAppLoaderInit");
  HLE::Patch(0x81300000, "AppLoaderReport");  // HLE OSReport for Apploader
  PowerPC::ppcState.gpr[3] = 0x81300000;
  RunFunction(iAppLoaderInit);

  // iAppLoaderMain - Here we load the apploader, the DOL (the exe) and the FST (filesystem).
  // To give you an idea about where the stuff is located on the disc take a look at yagcd
  // ch 13.
  DEBUG_LOG(MASTER_LOG, "Call iAppLoaderMain");
  do
  {
    PowerPC::ppcState.gpr[3] = 0x81300004;
    PowerPC::ppcState.gpr[4] = 0x81300008;
    PowerPC::ppcState.gpr[5] = 0x8130000c;

    RunFunction(iAppLoaderMain);

    u32 iRamAddress = PowerPC::Read_U32(0x81300004);
    u32 iLength = PowerPC::Read_U32(0x81300008);
    u32 iDVDOffset = PowerPC::Read_U32(0x8130000c) << (is_wii ? 2 : 0);

    INFO_LOG(MASTER_LOG, "DVDRead: offset: %08x   memOffset: %08x   length: %i", iDVDOffset,
             iRamAddress, iLength);
    DVDRead(volume, iDVDOffset, iRamAddress, iLength, partition);

  } while (PowerPC::ppcState.gpr[3] != 0x00);

  // iAppLoaderClose
  DEBUG_LOG(MASTER_LOG, "call iAppLoaderClose");
  RunFunction(iAppLoaderClose);
  HLE::UnPatch("AppLoaderReport");

  // return
  PC = PowerPC::ppcState.gpr[3];

  return true;
}

void CBoot::SetupGCMemory()
{
  // Booted from bootrom. 0xE5207C22 = booted from jtag
  PowerPC::HostWrite_U32(0x0D15EA5E, 0x80000020);

  // Physical Memory Size (24MB on retail)
  PowerPC::HostWrite_U32(Memory::REALRAM_SIZE, 0x80000028);

  // Console type - DevKit  (retail ID == 0x00000003) see YAGCD 4.2.1.1.2
  // TODO: determine why some games fail when using a retail ID.
  // (Seem to take different EXI paths, see Ikaruga for example)
  PowerPC::HostWrite_U32(0x10000006, 0x8000002C);

  // Fake the VI Init of the IPL (YAGCD 4.2.1.4)
  PowerPC::HostWrite_U32(DiscIO::IsNTSC(SConfig::GetInstance().m_region) ? 0 : 1, 0x800000CC);

  PowerPC::HostWrite_U32(0x01000000, 0x800000d0);  // ARAM Size. 16MB main + 4/16/32MB external
                                                   // (retail consoles have no external ARAM)

  PowerPC::HostWrite_U32(0x09a7ec80, 0x800000F8);  // Bus Clock Speed
  PowerPC::HostWrite_U32(0x1cf7c580, 0x800000FC);  // CPU Clock Speed

  PowerPC::HostWrite_U32(0x4c000064, 0x80000300);  // Write default DFI Handler:     rfi
  PowerPC::HostWrite_U32(0x4c000064, 0x80000800);  // Write default FPU Handler:     rfi
  PowerPC::HostWrite_U32(0x4c000064, 0x80000C00);  // Write default Syscall Handler: rfi

  PresetTimeBaseTicks();

  // HIO checks this
  // PowerPC::HostWrite_U16(0x8200, 0x000030e6);   // Console type
}

// __________________________________________________________________________________________________
// GameCube Bootstrap 2 HLE:
// copy the apploader to 0x81200000
// execute the apploader, function by function, using the above utility.
bool CBoot::EmulatedBS2_GC(const DiscIO::Volume& volume)
{
  INFO_LOG(BOOT, "Faking GC BS2...");

  SetupMSR();
  SetupBAT(/*is_wii*/ false);

  SetupGCMemory();

  DVDRead(volume, /*offset*/ 0x00000000, /*address*/ 0x00000000, 0x20, DiscIO::PARTITION_NONE);

  const bool ntsc = DiscIO::IsNTSC(SConfig::GetInstance().m_region);

  // Setup pointers like real BS2 does

  // StackPointer, used to be set to 0x816ffff0
  PowerPC::ppcState.gpr[1] = ntsc ? 0x81566550 : 0x815edca8;
  // Global pointer to Small Data Area 2 Base (haven't seen anything use it...meh)
  PowerPC::ppcState.gpr[2] = ntsc ? 0x81465cc0 : 0x814b5b20;
  // Global pointer to Small Data Area Base (Luigi's Mansion's apploader uses it)
  PowerPC::ppcState.gpr[13] = ntsc ? 0x81465320 : 0x814b4fc0;

  return RunApploader(/*is_wii*/ false, volume);
}

bool CBoot::SetupWiiMemory(u64 ios_title_id)
{
  static const std::map<DiscIO::Region, const RegionSetting> region_settings = {
      {DiscIO::Region::NTSC_J, {"JPN", "NTSC", "JP", "LJ"}},
      {DiscIO::Region::NTSC_U, {"USA", "NTSC", "US", "LU"}},
      {DiscIO::Region::PAL, {"EUR", "PAL", "EU", "LE"}},
      {DiscIO::Region::NTSC_K, {"KOR", "NTSC", "KR", "LKH"}}};
  auto entryPos = region_settings.find(SConfig::GetInstance().m_region);
  const RegionSetting& region_setting = entryPos->second;

  SettingsHandler gen;
  std::string serno;
  const std::string settings_file_path(
      Common::GetTitleDataPath(Titles::SYSTEM_MENU, Common::FROM_SESSION_ROOT) + WII_SETTING);
  if (File::Exists(settings_file_path) && gen.Open(settings_file_path))
  {
    serno = gen.GetValue("SERNO");
    gen.Reset();

    File::Delete(settings_file_path);
  }

  if (serno.empty() || serno == "000000000")
  {
    if (Core::WantsDeterminism())
      serno = "123456789";
    else
      serno = SettingsHandler::GenerateSerialNumber();
    INFO_LOG(BOOT, "No previous serial number found, generated one instead: %s", serno.c_str());
  }
  else
  {
    INFO_LOG(BOOT, "Using serial number: %s", serno.c_str());
  }

  std::string model = "RVL-001(" + region_setting.area + ")";
  gen.AddSetting("AREA", region_setting.area);
  gen.AddSetting("MODEL", model);
  gen.AddSetting("DVD", "0");
  gen.AddSetting("MPCH", "0x7FFE");
  gen.AddSetting("CODE", region_setting.code);
  gen.AddSetting("SERNO", serno);
  gen.AddSetting("VIDEO", region_setting.video);
  gen.AddSetting("GAME", region_setting.game);

  if (!gen.Save(settings_file_path))
  {
    PanicAlertT("SetupWiiMemory: Can't create setting.txt file");
    return false;
  }

  // Write the 256 byte setting.txt to memory.
  Memory::CopyToEmu(0x3800, gen.GetData(), SettingsHandler::SETTINGS_SIZE);

  INFO_LOG(BOOT, "Setup Wii Memory...");

  /*
  Set hardcoded global variables to Wii memory. These are partly collected from
  WiiBrew. These values are needed for the games to function correctly. A few
  values in this region will also be placed here by the game as it boots.
  They are:
  0x80000038  Start of FST
  0x8000003c  Size of FST Size
  0x80000060  Copyright code
  */

  Memory::Write_U32(0x0D15EA5E, 0x00000020);            // Another magic word
  Memory::Write_U32(0x00000001, 0x00000024);            // Unknown
  Memory::Write_U32(Memory::REALRAM_SIZE, 0x00000028);  // MEM1 size 24MB
  Memory::Write_U32(0x00000023, 0x0000002c);            // Production Board Model
  Memory::Write_U32(0x00000000, 0x00000030);            // Init
  Memory::Write_U32(0x817FEC60, 0x00000034);            // Init
  // 38, 3C should get start, size of FST through apploader
  Memory::Write_U32(0x8008f7b8, 0x000000e4);            // Thread Init
  Memory::Write_U32(Memory::REALRAM_SIZE, 0x000000f0);  // "Simulated memory size" (debug mode?)
  Memory::Write_U32(0x8179b500, 0x000000f4);            // __start
  Memory::Write_U32(0x0e7be2c0, 0x000000f8);            // Bus speed
  Memory::Write_U32(0x2B73A840, 0x000000fc);            // CPU speed
  Memory::Write_U16(0x0000, 0x000030e6);                // Console type
  Memory::Write_U32(0x00000000, 0x000030c0);            // EXI
  Memory::Write_U32(0x00000000, 0x000030c4);            // EXI
  Memory::Write_U32(0x00000000, 0x000030dc);            // Time
  Memory::Write_U32(0xffffffff, 0x000030d8);            // Unknown, set by any official NAND title
  Memory::Write_U16(0x8201, 0x000030e6);                // Dev console / debug capable
  Memory::Write_U32(0x00000000, 0x000030f0);            // Apploader

  // During the boot process, 0x315c is first set to 0xdeadbeef by IOS
  // in the boot_ppc syscall. The value is then partly overwritten by SDK titles.
  // Two bytes at 0x315e are used to indicate the "devkit boot program version".
  // It is only written to by the system menu, so we must do so here as well.
  //
  // 0x0113 appears to mean v1.13, which is the latest version.
  // It is fine to always use the latest value as apploaders work with all versions.
  Memory::Write_U16(0x0113, 0x0000315e);

  if (!IOS::HLE::GetIOS()->BootIOS(ios_title_id))
    return false;

  Memory::Write_U8(0x80, 0x0000315c);         // OSInit
  Memory::Write_U16(0x0000, 0x000030e0);      // PADInit
  Memory::Write_U32(0x80000000, 0x00003184);  // GameID Address

  // Fake the VI Init of the IPL
  Memory::Write_U32(DiscIO::IsNTSC(SConfig::GetInstance().m_region) ? 0 : 1, 0x000000CC);

  // Clear exception handler. Why? Don't we begin with only zeros?
  for (int i = 0x3000; i <= 0x3038; i += 4)
  {
    Memory::Write_U32(0x00000000, i);
  }

  return true;
}

static void WriteEmptyPlayRecord()
{
  const std::string file_path =
      Common::GetTitleDataPath(Titles::SYSTEM_MENU, Common::FROM_SESSION_ROOT) + "play_rec.dat";
  File::IOFile playrec_file(file_path, "r+b");
  std::vector<u8> empty_record(0x80);
  playrec_file.WriteBytes(empty_record.data(), empty_record.size());
}

// __________________________________________________________________________________________________
// Wii Bootstrap 2 HLE:
// copy the apploader to 0x81200000
// execute the apploader
bool CBoot::EmulatedBS2_Wii(const DiscIO::Volume& volume)
{
  INFO_LOG(BOOT, "Faking Wii BS2...");
  if (volume.GetVolumeType() != DiscIO::Platform::WII_DISC)
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

  // While reading a disc, the system menu reads the first partition table
  // (0x20 bytes from 0x00040020) and stores a pointer to the data partition entry.
  // When launching the disc game, it copies the partition type and offset to 0x3194
  // and 0x3198 respectively.
  const DiscIO::Partition data_partition = volume.GetGamePartition();
  Memory::Write_U32(0, 0x3194);
  Memory::Write_U32(static_cast<u32>(data_partition.offset >> 2), 0x3198);

  if (!SetupWiiMemory(tmd.GetIOSId()))
    return false;

  DVDRead(volume, 0x00000000, 0x00000000, 0x20, DiscIO::PARTITION_NONE);  // Game Code

  // This is some kind of consistency check that is compared to the 0x00
  // values as the game boots. This location keeps the 4 byte ID for as long
  // as the game is running. The 6 byte ID at 0x00 is overwritten sometime
  // after this check during booting.
  DVDRead(volume, 0, 0x3180, 4, partition);

  SetupMSR();
  SetupBAT(/*is_wii*/ true);

  Memory::Write_U32(0x4c000064, 0x00000300);  // Write default DSI Handler:   rfi
  Memory::Write_U32(0x4c000064, 0x00000800);  // Write default FPU Handler:   rfi
  Memory::Write_U32(0x4c000064, 0x00000C00);  // Write default Syscall Handler: rfi

  PowerPC::ppcState.gpr[1] = 0x816ffff0;  // StackPointer

  if (!RunApploader(/*is_wii*/ true, volume))
    return false;

  // Warning: This call will set incorrect running game metadata if our volume parameter
  // doesn't point to the same disc as the one that's inserted in the emulated disc drive!
  IOS::HLE::Device::ES::DIVerify(tmd, volume.GetTicket(partition));

  return true;
}

// Returns true if apploader has run successfully. If is_wii is true, the disc
// that volume refers to must currently be inserted into the emulated disc drive.
bool CBoot::EmulatedBS2(bool is_wii, const DiscIO::Volume& volume)
{
  return is_wii ? EmulatedBS2_Wii(volume) : EmulatedBS2_GC(volume);
}
