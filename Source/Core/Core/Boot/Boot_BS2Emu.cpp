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
#include "Core/Debugger/BranchWatch.h"
#include "Core/HLE/HLE.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/EXI/EXI_DeviceIPL.h"
#include "Core/HW/Memmap.h"
#include "Core/Host.h"
#include "Core/IOS/DI/DI.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/IOS/FS/FileSystem.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/IOSC.h"
#include "Core/IOS/Uids.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

#include "DiscIO/Enums.h"
#include "DiscIO/RiivolutionPatcher.h"
#include "DiscIO/VolumeDisc.h"

#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/XFMemory.h"
#include "VideoCommon/XFStateManager.h"

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

void CBoot::SetupMSR(PowerPC::PowerPCState& ppc_state)
{
  // 0x0002032
  ppc_state.msr.RI = 1;
  ppc_state.msr.DR = 1;
  ppc_state.msr.IR = 1;
  ppc_state.msr.FP = 1;
  PowerPC::MSRUpdated(ppc_state);
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

/*
#include <ogc/machine/asm.h>

  .globl AppLoaderRunnerBase
AppLoaderRunnerBase:
  b RunAppLoader

// Variables that iAppLoaderMain stores into
AppLoaderMainRamAddress:
.4byte 0x00dead04
AppLoaderMainLength:
.4byte 0x00dead08
AppLoaderMainDiscOffset:
.4byte 0x00dead0c

// Function pointers returned by iAppLoaderEntry
AppLoaderInitPointer:  // pointer to: void iAppLoaderInit(void (*OSReport)(char *fmt, ...))
.4byte 0x00dead10
AppLoaderMainPointer:  // pointer to: bool iAppLoaderMain(u32* ram_addr, u32* len, u32* disc_offset)
.4byte 0x00dead14
AppLoaderClosePointer:  // pointer to: void* iAppLoaderClose(void);
.4byte 0x00dead18

HLEAppLoaderAfterEntry:  // void HLEAppLoaderAfterEntry(void), logging
  blr
HLEAppLoaderAfterInit:  // void HLEAppLoaderAfterInit(void), logging
  blr
HLEAppLoaderAfterMain:  // void HLEAppLoaderAfterMain(void), reads disc at AppLoaderMain* variables
  blr
HLEAppLoaderAfterClose:  // void HLEAppLoaderAfterClose(void), cleans up HLE state
  blr
HLEAppLoaderReport:  // void HLEAppLoaderReport(char *fmt, ...), passed to iAppLoaderInit
  blr

RunAppLoader:
  // Entry point; call iAppLoaderEntry
  // r31 is AppLoaderRunnerBase
  // r30 is iAppLoaderEntry (later used as a scratch register for other mtctr + bctrl sequences)
  // Note that the above registers are specific to Dolphin's apploader HLE.
  addi r3, r31, (AppLoaderInitPointer - AppLoaderRunnerBase)
  addi r4, r31, (AppLoaderMainPointer - AppLoaderRunnerBase)
  addi r5, r31, (AppLoaderClosePointer - AppLoaderRunnerBase)
  mtctr r30
  bctrl  // Call iAppLoaderEntry, which looks like this:
  // void iAppLoaderEntry(void** init, void** main, void** close)
  // {
  //   *init = &iAppLoaderInit;
  //   *main = &iAppLoaderMain;
  //   *close = &iAppLoaderClose;
  // }
  bl HLEAppLoaderAfterEntry  // Logging only

  // Call iAppLoaderInit, which sets up logging and any internal state.
  addi r3, r31, (HLEAppLoaderReport - AppLoaderRunnerBase)
  lwz r30, (AppLoaderInitPointer - AppLoaderRunnerBase)(r31)
  mtctr r30
  bctrl  // Call iAppLoaderInit
  bl HLEAppLoaderAfterInit  // Logging only

  // iAppLoaderMain - Here we load the apploader, the DOL (the exe) and the FST (filesystem).
  // To give you an idea about where the stuff is located on the disc take a look at yagcd
  // ch 13.
  // iAppLoaderMain returns 1 if the pointers in R3/R4/R5 were filled with values for DVD copy
  // Typical behaviour is doing it once for each section defined in the DOL header. Some unlicensed
  // titles don't have a properly constructed DOL and maintain a table of these values in apploader.
  // iAppLoaderMain returns 0 when there are no more sections to copy.
LoopMain:
  addi r3, r31, (AppLoaderMainRamAddress - AppLoaderRunnerBase)
  addi r4, r31, (AppLoaderMainLength - AppLoaderRunnerBase)
  addi r5, r31, (AppLoaderMainDiscOffset - AppLoaderRunnerBase)
  lwz r30, (AppLoaderMainPointer - AppLoaderRunnerBase)(r31)
  mtctr r30
  bctrl  // Call iAppLoaderMain
  cmpwi r3,0
  beq LoopMainDone
  bl HLEAppLoaderAfterMain  // Reads the disc (and logs)
  b LoopMain

  // Call iAppLoaderClose, which returns (in r3) the game's entry point
LoopMainDone:
  lwz r30, (AppLoaderClosePointer - AppLoaderRunnerBase)(r31)
  mtctr r30
  bctrl  // Call iAppLoaderClose
  mr r30, r3
  bl HLEAppLoaderAfterClose  // Unpatches the HLE functions
  mtlr r30
  blr  // "Return" to the game's entry point; this allows using "step out" to skip the apploader
*/
static constexpr u32 APPLOADER_RUNNER_BASE = 0x81300000;
static constexpr u32 APPLOADER_RUNNER_MAIN_RAM_ADDR = APPLOADER_RUNNER_BASE + 0x04;
static constexpr u32 APPLOADER_RUNNER_MAIN_LENGTH = APPLOADER_RUNNER_BASE + 0x08;
static constexpr u32 APPLOADER_RUNNER_MAIN_DISC_OFFSET = APPLOADER_RUNNER_BASE + 0x0c;
static constexpr u32 APPLOADER_RUNNER_INIT_POINTER = APPLOADER_RUNNER_BASE + 0x10;
static constexpr u32 APPLOADER_RUNNER_MAIN_POINTER = APPLOADER_RUNNER_BASE + 0x14;
static constexpr u32 APPLOADER_RUNNER_CLOSE_POINTER = APPLOADER_RUNNER_BASE + 0x18;
static constexpr u32 APPLOADER_RUNNER_HLE_AFTER_ENTRY = APPLOADER_RUNNER_BASE + 0x1c;
static constexpr u32 APPLOADER_RUNNER_HLE_AFTER_INIT = APPLOADER_RUNNER_BASE + 0x20;
static constexpr u32 APPLOADER_RUNNER_HLE_AFTER_MAIN = APPLOADER_RUNNER_BASE + 0x24;
static constexpr u32 APPLOADER_RUNNER_HLE_AFTER_CLOSE = APPLOADER_RUNNER_BASE + 0x28;
static constexpr u32 APPLOADER_RUNNER_HLE_REPORT = APPLOADER_RUNNER_BASE + 0x2c;
static constexpr u32 APPLOADER_RUNNER_RUN_ADDR = APPLOADER_RUNNER_BASE + 0x30;

static constexpr std::array<u32, 40> APPLOADER_RUNNER_ASM{
    0x48000030,  // Starting thunk
    // Variables
    0x00dead04, 0x00dead08, 0x00dead0c, 0x00dead10, 0x00dead14, 0x00dead18,
    // HLE'd functions
    0x4e800020, 0x4e800020, 0x4e800020, 0x4e800020, 0x4e800020,
    // Actual code
    0x387f0010, 0x389f0014, 0x38bf0018, 0x7fc903a6, 0x4e800421, 0x4bffffd9, 0x387f002c, 0x83df0010,
    0x7fc903a6, 0x4e800421, 0x4bffffc9, 0x387f0004, 0x389f0008, 0x38bf000c, 0x83df0014, 0x7fc903a6,
    0x4e800421, 0x2c030000, 0x4182000c, 0x4bffffa9, 0x4bffffdc, 0x83df0018, 0x7fc903a6, 0x4e800421,
    0x7c7e1b78, 0x4bffff95, 0x7fc803a6, 0x4e800020};

struct ApploaderRunnerState
{
  constexpr ApploaderRunnerState(
      bool is_wii_, const DiscIO::VolumeDisc& volume_,
      const std::vector<DiscIO::Riivolution::Patch>& riivolution_patches_)
      : is_wii(is_wii_), volume(volume_), riivolution_patches(riivolution_patches_)
  {
  }

  bool is_wii;
  const DiscIO::VolumeDisc& volume;
  const std::vector<DiscIO::Riivolution::Patch>& riivolution_patches;
};
static std::unique_ptr<ApploaderRunnerState> s_apploader_runner_state;

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
  INFO_LOG_FMT(BOOT,
               "Loading apploader ({}) from disc offset {:08x} to {:08x}, "
               "size {:#x} ({:#x} + {:#x}), entry point {:08x}",
               volume.GetApploaderDate(partition), offset + 0x20, 0x81200000, *size + *trailer,
               *size, *trailer, *entry);
  DVDRead(system, volume, offset + 0x20, 0x01200000, *size + *trailer, partition);

  auto& mmu = system.GetMMU();
  auto& branch_watch = system.GetPowerPC().GetBranchWatch();

  const bool resume_branch_watch = branch_watch.GetRecordingActive();
  if (system.IsBranchWatchIgnoreApploader())
    branch_watch.SetRecordingActive(guard, false);

  for (u32 i = 0; i < APPLOADER_RUNNER_ASM.size(); i++)
  {
    mmu.HostWrite_U32(guard, APPLOADER_RUNNER_ASM[i], APPLOADER_RUNNER_BASE + i * sizeof(u32));
  }

  HLE::Patch(system, APPLOADER_RUNNER_HLE_AFTER_ENTRY, "HLEAppLoaderAfterEntry");
  HLE::Patch(system, APPLOADER_RUNNER_HLE_AFTER_INIT, "HLEAppLoaderAfterInit");
  HLE::Patch(system, APPLOADER_RUNNER_HLE_AFTER_MAIN, "HLEAppLoaderAfterMain");
  HLE::Patch(system, APPLOADER_RUNNER_HLE_AFTER_CLOSE, "HLEAppLoaderAfterClose");
  HLE::Patch(system, APPLOADER_RUNNER_HLE_REPORT, "HLEAppLoaderReport");
  s_apploader_runner_state =
      std::make_unique<ApploaderRunnerState>(is_wii, volume, riivolution_patches);

  system.GetPPCSymbolDB().AddKnownSymbol(guard, *entry, 0, "iAppLoaderEntry", "Apploader");

  system.GetPPCSymbolDB().AddKnownSymbol(guard, APPLOADER_RUNNER_MAIN_RAM_ADDR, 4,
                                         "AppLoaderMainRamAddress", "Apploader",
                                         Common::Symbol::Type::Data);
  system.GetPPCSymbolDB().AddKnownSymbol(guard, APPLOADER_RUNNER_MAIN_LENGTH, 4,
                                         "AppLoaderMainLength", "Apploader",
                                         Common::Symbol::Type::Data);
  system.GetPPCSymbolDB().AddKnownSymbol(guard, APPLOADER_RUNNER_MAIN_DISC_OFFSET, 4,
                                         "AppLoaderMainDiscOffset", "Apploader",
                                         Common::Symbol::Type::Data);
  system.GetPPCSymbolDB().AddKnownSymbol(guard, APPLOADER_RUNNER_INIT_POINTER, 4,
                                         "AppLoaderInitPointer", "Apploader",
                                         Common::Symbol::Type::Data);
  system.GetPPCSymbolDB().AddKnownSymbol(guard, APPLOADER_RUNNER_MAIN_POINTER, 4,
                                         "AppLoaderMainPointer", "Apploader",
                                         Common::Symbol::Type::Data);
  system.GetPPCSymbolDB().AddKnownSymbol(guard, APPLOADER_RUNNER_CLOSE_POINTER, 4,
                                         "AppLoaderClosePointer", "Apploader",
                                         Common::Symbol::Type::Data);

  system.GetPPCSymbolDB().AddKnownSymbol(guard, APPLOADER_RUNNER_HLE_AFTER_ENTRY, 4,
                                         "HLEAppLoaderAfterEntry", "Dolphin BS2 HLE");
  system.GetPPCSymbolDB().AddKnownSymbol(guard, APPLOADER_RUNNER_HLE_AFTER_INIT, 4,
                                         "HLEAppLoaderAfterInit", "Dolphin BS2 HLE");
  system.GetPPCSymbolDB().AddKnownSymbol(guard, APPLOADER_RUNNER_HLE_AFTER_MAIN, 4,
                                         "HLEAppLoaderAfterMain", "Dolphin BS2 HLE");
  system.GetPPCSymbolDB().AddKnownSymbol(guard, APPLOADER_RUNNER_HLE_AFTER_CLOSE, 4,
                                         "HLEAppLoaderAfterClose", "Dolphin BS2 HLE");
  system.GetPPCSymbolDB().AddKnownSymbol(guard, APPLOADER_RUNNER_HLE_REPORT, 4,
                                         "HLEAppLoaderReport", "Dolphin BS2 HLE");

  system.GetPPCSymbolDB().AddKnownSymbol(guard, APPLOADER_RUNNER_BASE, 4, "DolphinApploaderRunner",
                                         "Dolphin BS2 HLE");
  const u32 remainder_size = u32(APPLOADER_RUNNER_ASM.size() * sizeof(u32)) -
                             (APPLOADER_RUNNER_RUN_ADDR - APPLOADER_RUNNER_BASE);
  system.GetPPCSymbolDB().AddKnownSymbol(guard, APPLOADER_RUNNER_RUN_ADDR, remainder_size,
                                         "DolphinApploaderRunner", "Dolphin BS2 HLE");
  system.GetPPCSymbolDB().Index();
  Host_PPCSymbolsChanged();

  auto& ppc_state = system.GetPPCState();
  ppc_state.gpr[31] = APPLOADER_RUNNER_BASE;
  ppc_state.gpr[30] = *entry;

  // Run our apploader runner code
  ppc_state.pc = APPLOADER_RUNNER_BASE;

  branch_watch.SetRecordingActive(guard, resume_branch_watch);
  // Blank out session key (https://debugmo.de/2008/05/part-2-dumping-the-media-board/)
  if (volume.GetVolumeType() == DiscIO::Platform::Triforce)
  {
    auto& memory = system.GetMemory();

    memory.Memset(0, 0, 12);
  }

  return true;
}

void CBoot::HLEAppLoaderAfterEntry(const Core::CPUThreadGuard& guard)
{
  auto& system = Core::System::GetInstance();
  auto& mmu = system.GetMMU();

  const u32 init = mmu.HostRead_U32(guard, APPLOADER_RUNNER_INIT_POINTER);
  const u32 main = mmu.HostRead_U32(guard, APPLOADER_RUNNER_MAIN_POINTER);
  const u32 close = mmu.HostRead_U32(guard, APPLOADER_RUNNER_CLOSE_POINTER);

  system.GetPPCSymbolDB().AddKnownSymbol(guard, init, 0, "iAppLoaderInit", "Apploader");
  system.GetPPCSymbolDB().AddKnownSymbol(guard, main, 0, "iAppLoaderMain", "Apploader");
  system.GetPPCSymbolDB().AddKnownSymbol(guard, close, 0, "iAppLoaderClose", "Apploader");
  system.GetPPCSymbolDB().Index();
  Host_PPCSymbolsChanged();

  INFO_LOG_FMT(BOOT, "Called iAppLoaderEntry; init at {:08x}, main at {:08x}, close at {:08x}",
               init, main, close);
}

void CBoot::HLEAppLoaderAfterInit(const Core::CPUThreadGuard& guard)
{
  INFO_LOG_FMT(BOOT, "Called iAppLoaderInit");
}

void CBoot::HLEAppLoaderAfterMain(const Core::CPUThreadGuard& guard)
{
  ASSERT(s_apploader_runner_state);
  if (!s_apploader_runner_state)
    return;

  auto& system = Core::System::GetInstance();
  auto& mmu = system.GetMMU();

  const u32 ram_address = mmu.HostRead_U32(guard, APPLOADER_RUNNER_MAIN_RAM_ADDR);
  const u32 length = mmu.HostRead_U32(guard, APPLOADER_RUNNER_MAIN_LENGTH);
  const u32 dvd_shift = s_apploader_runner_state->is_wii ? 2 : 0;
  const u32 dvd_offset = mmu.HostRead_U32(guard, APPLOADER_RUNNER_MAIN_DISC_OFFSET) << dvd_shift;

  INFO_LOG_FMT(BOOT, "Called iAppLoaderMain; reading {:#x} bytes from disc offset {:08x} to {:08x}",
               length, dvd_offset, ram_address);
  DVDRead(system, s_apploader_runner_state->volume, dvd_offset, ram_address, length,
          s_apploader_runner_state->volume.GetGamePartition());
  DiscIO::Riivolution::ApplyApploaderMemoryPatches(
      guard, s_apploader_runner_state->riivolution_patches, ram_address, length);
}

void CBoot::HLEAppLoaderAfterClose(const Core::CPUThreadGuard& guard)
{
  auto& system = Core::System::GetInstance();
  auto& ppc_state = system.GetPPCState();

  INFO_LOG_FMT(BOOT, "Called iAppLoaderClose; game entry point is {:08x}", ppc_state.gpr[30]);
  SConfig::OnTitleDirectlyBooted(guard);  // Clears the temporary patches and symbols
}

void CBoot::HLEAppLoaderReport(std::string_view message)
{
  auto& system = Core::System::GetInstance();
  auto& ppc_state = system.GetPPCState();

  INFO_LOG_FMT(BOOT, "AppLoader {:08x}->{:08x}| {}", LR(ppc_state), ppc_state.pc, message);
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
  auto& xf_state_manager = system.GetXFStateManager();
  xf_state_manager.InvalidateXFRange(XFMEM_POSTMATRICES + 0x3d * 4, XFMEM_POSTMATRICES_END);

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

  std::string serno;
  std::string model = "RVL-001(" + region_setting.area + ")";
  CreateSystemMenuTitleDirs();
  const std::string settings_file_path(Common::GetTitleDataPath(Titles::SYSTEM_MENU) +
                                       "/" WII_SETTING);

  const auto fs = system.GetIOS()->GetFS();
  {
    Common::SettingsBuffer data;
    const auto file = fs->OpenFile(IOS::SYSMENU_UID, IOS::SYSMENU_GID, settings_file_path,
                                   IOS::HLE::FS::Mode::Read);
    if (file && file->Read(data.data(), data.size()))
    {
      const Common::SettingsReader settings_reader(data);
      serno = settings_reader.GetValue("SERNO");
      model = settings_reader.GetValue("MODEL");

      bool region_matches = false;
      if (Config::Get(Config::MAIN_OVERRIDE_REGION_SETTINGS))
      {
        region_matches = true;
      }
      else
      {
        const std::string code = settings_reader.GetValue("CODE");
        if (code.size() >= 2 && CodeRegion(code[1]) == SConfig::GetInstance().m_region)
          region_matches = true;
      }

      if (region_matches)
      {
        region_setting =
            RegionSetting{settings_reader.GetValue("AREA"), settings_reader.GetValue("VIDEO"),
                          settings_reader.GetValue("GAME"), settings_reader.GetValue("CODE")};
      }
      else
      {
        const size_t parenthesis_pos = model.find('(');
        if (parenthesis_pos != std::string::npos)
          model = model.substr(0, parenthesis_pos) + '(' + region_setting.area + ')';
      }
    }
  }
  fs->Delete(IOS::SYSMENU_UID, IOS::SYSMENU_GID, settings_file_path);

  if (serno.empty() || serno == "000000000")
  {
    if (Core::WantsDeterminism())
      serno = "123456789";
    else
      serno = Common::SettingsWriter::GenerateSerialNumber();
    INFO_LOG_FMT(BOOT, "No previous serial number found, generated one instead: {}", serno);
  }
  else
  {
    INFO_LOG_FMT(BOOT, "Using serial number: {}", serno);
  }

  Common::SettingsWriter settings_writer;
  settings_writer.AddSetting("AREA", region_setting.area);
  settings_writer.AddSetting("MODEL", model);
  settings_writer.AddSetting("DVD", "0");
  settings_writer.AddSetting("MPCH", "0x7FFE");
  settings_writer.AddSetting("CODE", region_setting.code);
  settings_writer.AddSetting("SERNO", serno);
  settings_writer.AddSetting("VIDEO", region_setting.video);
  settings_writer.AddSetting("GAME", region_setting.game);

  constexpr IOS::HLE::FS::Mode rw_mode = IOS::HLE::FS::Mode::ReadWrite;
  const auto settings_file = fs->CreateAndOpenFile(IOS::SYSMENU_UID, IOS::SYSMENU_GID,
                                                   settings_file_path, {rw_mode, rw_mode, rw_mode});
  if (!settings_file ||
      !settings_file->Write(settings_writer.GetBytes().data(), settings_writer.GetBytes().size()))
  {
    PanicAlertFmtT("SetupWiiMemory: Can't create setting.txt file");
    return false;
  }

  auto& memory = system.GetMemory();

  // Write the 256 byte setting.txt to memory.
  memory.CopyToEmu(0x3800, settings_writer.GetBytes().data(), settings_writer.GetBytes().size());

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
  const auto fs = Core::System::GetInstance().GetIOS()->GetFS();
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
  if (!SetupWiiMemory(system, console_type) || !system.GetIOS()->BootIOS(ios))
    return false;

  auto di =
      std::static_pointer_cast<IOS::HLE::DIDevice>(system.GetIOS()->GetDeviceByName("/dev/di"));

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
  IOS::HLE::RAMOverrideForIOSMemoryValues(memory, IOS::HLE::MemorySetupType::IOSReload);

  // Warning: This call will set incorrect running game metadata if our volume parameter
  // doesn't point to the same disc as the one that's inserted in the emulated disc drive!
  system.GetIOS()->GetESDevice()->DIVerify(tmd, volume.GetTicket(partition));

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
