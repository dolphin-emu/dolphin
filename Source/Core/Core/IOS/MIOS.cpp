// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/MIOS.h"

#include <cstring>
#include <utility>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/NandPaths.h"
#include "Common/Swap.h"
#include "Core/Boot/ElfReader.h"
#include "Core/CommonTitles.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/DSPEmulator.h"
#include "Core/HLE/HLE.h"
#include "Core/HW/DSP.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SystemTimers.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"
#include "DiscIO/NANDContentLoader.h"

namespace IOS
{
namespace HLE
{
namespace MIOS
{
// Source: https://wiibrew.org/wiki/ARM_Binaries
struct ARMBinary final
{
  explicit ARMBinary(const std::vector<u8>& bytes);
  explicit ARMBinary(std::vector<u8>&& bytes);

  bool IsValid() const;
  std::vector<u8> GetElf() const;
  u32 GetHeaderSize() const;
  u32 GetElfOffset() const;
  u32 GetElfSize() const;

private:
  std::vector<u8> m_bytes;
};

ARMBinary::ARMBinary(const std::vector<u8>& bytes) : m_bytes(bytes)
{
}

ARMBinary::ARMBinary(std::vector<u8>&& bytes) : m_bytes(std::move(bytes))
{
}

bool ARMBinary::IsValid() const
{
  // The header is at least 0x10.
  if (m_bytes.size() < 0x10)
    return false;
  return m_bytes.size() >= (GetHeaderSize() + GetElfOffset() + GetElfSize());
}

std::vector<u8> ARMBinary::GetElf() const
{
  const auto iterator = m_bytes.cbegin() + GetHeaderSize() + GetElfOffset();
  return std::vector<u8>(iterator, iterator + GetElfSize());
}

u32 ARMBinary::GetHeaderSize() const
{
  return Common::swap32(m_bytes.data());
}

u32 ARMBinary::GetElfOffset() const
{
  return Common::swap32(m_bytes.data() + 0x4);
}

u32 ARMBinary::GetElfSize() const
{
  return Common::swap32(m_bytes.data() + 0x8);
}

static std::vector<u8> GetMIOSBinary()
{
  const auto& loader =
      DiscIO::NANDContentManager::Access().GetNANDLoader(Titles::MIOS, Common::FROM_SESSION_ROOT);
  if (!loader.IsValid())
    return {};

  const auto* content = loader.GetContentByIndex(loader.GetTMD().GetBootIndex());
  if (!content)
    return {};

  return content->m_Data->Get();
}

static void ReinitHardware()
{
  SConfig::GetInstance().bWii = false;

  // IOS clears mem2 and overwrites it with pseudo-random data (for security).
  std::memset(Memory::m_pEXRAM, 0, Memory::EXRAM_SIZE);
  // MIOS appears to only reset the DI and the PPC.
  DVDInterface::Reset();
  PowerPC::Reset();
  // Note: this is specific to Dolphin and is required because we initialised it in Wii mode.
  DSP::Reinit(SConfig::GetInstance().bDSPHLE);
  DSP::GetDSPEmulator()->Initialize(SConfig::GetInstance().bWii, SConfig::GetInstance().bDSPThread);

  SystemTimers::ChangePPCClock(SystemTimers::Mode::GC);
}

constexpr u32 ADDRESS_INIT_SEMAPHORE = 0x30f8;

bool Load()
{
  Memory::Write_U32(0x00000000, ADDRESS_INIT_SEMAPHORE);
  Memory::Write_U32(0x09142001, 0x3180);

  ARMBinary mios{GetMIOSBinary()};
  if (!mios.IsValid())
  {
    PanicAlertT("Failed to load MIOS. It is required for launching GameCube titles from Wii mode.");
    Core::QueueHostJob(Core::Stop);
    return false;
  }

  ElfReader elf{mios.GetElf()};
  if (!elf.LoadIntoMemory(true))
  {
    PanicAlertT("Failed to load MIOS ELF into memory.");
    Core::QueueHostJob(Core::Stop);
    return false;
  }

  ReinitHardware();
  NOTICE_LOG(IOS, "Reinitialised hardware.");

  // Load symbols for the IPL if they exist.
  g_symbolDB.Clear();
  if (g_symbolDB.LoadMap(File::GetUserPath(D_MAPS_IDX) + "mios-ipl.map"))
  {
    ::HLE::Clear();
    ::HLE::PatchFunctions();
  }

  const PowerPC::CoreMode core_mode = PowerPC::GetMode();
  PowerPC::SetMode(PowerPC::CoreMode::Interpreter);
  MSR = 0;
  PC = 0x3400;
  NOTICE_LOG(IOS, "Loaded MIOS and bootstrapped PPC.");

  // IOS writes 0 to 0x30f8 before bootstrapping the PPC. Once started, the IPL eventually writes
  // 0xdeadbeef there, then waits for it to be cleared by IOS before continuing.
  while (Memory::Read_U32(ADDRESS_INIT_SEMAPHORE) != 0xdeadbeef)
    PowerPC::SingleStep();
  PowerPC::SetMode(core_mode);

  Memory::Write_U32(0x00000000, ADDRESS_INIT_SEMAPHORE);
  NOTICE_LOG(IOS, "IPL ready.");
  SConfig::GetInstance().m_is_mios = true;
  DVDInterface::UpdateRunningGameMetadata();
  return true;
}
}  // namespace MIOS
}  // namespace HLE
}  // namespace IOS
