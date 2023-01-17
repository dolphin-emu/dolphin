// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/DSPHLE/UCodes/UCodes.h"

#include <cstring>
#include <memory>
#include <string>

#ifdef _WIN32
#include <Windows.h>
#endif

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Hash.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/DSP/DSPCodeUtil.h"
#include "Core/HW/DSPHLE/DSPHLE.h"
#include "Core/HW/DSPHLE/UCodes/AESnd.h"
#include "Core/HW/DSPHLE/UCodes/ASnd.h"
#include "Core/HW/DSPHLE/UCodes/AX.h"
#include "Core/HW/DSPHLE/UCodes/AXWii.h"
#include "Core/HW/DSPHLE/UCodes/CARD.h"
#include "Core/HW/DSPHLE/UCodes/GBA.h"
#include "Core/HW/DSPHLE/UCodes/INIT.h"
#include "Core/HW/DSPHLE/UCodes/ROM.h"
#include "Core/HW/DSPHLE/UCodes/Zelda.h"
#include "Core/HW/Memmap.h"
#include "Core/System.h"

namespace DSP::HLE
{
constexpr bool ExramRead(u32 address)
{
  return (address & 0x10000000) != 0;
}

u8 HLEMemory_Read_U8(u32 address)
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();

  if (ExramRead(address))
    return memory.GetEXRAM()[address & memory.GetExRamMask()];

  return memory.GetRAM()[address & memory.GetRamMask()];
}

void HLEMemory_Write_U8(u32 address, u8 value)
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();

  if (ExramRead(address))
    memory.GetEXRAM()[address & memory.GetExRamMask()] = value;
  else
    memory.GetRAM()[address & memory.GetRamMask()] = value;
}

u16 HLEMemory_Read_U16LE(u32 address)
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();

  u16 value;

  if (ExramRead(address))
    std::memcpy(&value, &memory.GetEXRAM()[address & memory.GetExRamMask()], sizeof(u16));
  else
    std::memcpy(&value, &memory.GetRAM()[address & memory.GetRamMask()], sizeof(u16));

  return value;
}

u16 HLEMemory_Read_U16(u32 address)
{
  return Common::swap16(HLEMemory_Read_U16LE(address));
}

void HLEMemory_Write_U16LE(u32 address, u16 value)
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();

  if (ExramRead(address))
    std::memcpy(&memory.GetEXRAM()[address & memory.GetExRamMask()], &value, sizeof(u16));
  else
    std::memcpy(&memory.GetRAM()[address & memory.GetRamMask()], &value, sizeof(u16));
}

void HLEMemory_Write_U16(u32 address, u16 value)
{
  HLEMemory_Write_U16LE(address, Common::swap16(value));
}

u32 HLEMemory_Read_U32LE(u32 address)
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();

  u32 value;

  if (ExramRead(address))
    std::memcpy(&value, &memory.GetEXRAM()[address & memory.GetExRamMask()], sizeof(u32));
  else
    std::memcpy(&value, &memory.GetRAM()[address & memory.GetRamMask()], sizeof(u32));

  return value;
}

u32 HLEMemory_Read_U32(u32 address)
{
  return Common::swap32(HLEMemory_Read_U32LE(address));
}

void HLEMemory_Write_U32LE(u32 address, u32 value)
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();

  if (ExramRead(address))
    std::memcpy(&memory.GetEXRAM()[address & memory.GetExRamMask()], &value, sizeof(u32));
  else
    std::memcpy(&memory.GetRAM()[address & memory.GetRamMask()], &value, sizeof(u32));
}

void HLEMemory_Write_U32(u32 address, u32 value)
{
  HLEMemory_Write_U32LE(address, Common::swap32(value));
}

void* HLEMemory_Get_Pointer(u32 address)
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();

  if (ExramRead(address))
    return &memory.GetEXRAM()[address & memory.GetExRamMask()];

  return &memory.GetRAM()[address & memory.GetRamMask()];
}

UCodeInterface::UCodeInterface(DSPHLE* dsphle, u32 crc)
    : m_mail_handler(dsphle->AccessMailHandler()), m_dsphle(dsphle), m_crc(crc)
{
}

UCodeInterface::~UCodeInterface() = default;

bool UCodeInterface::NeedsResumeMail()
{
  if (m_needs_resume_mail)
  {
    m_needs_resume_mail = false;
    return true;
  }
  return false;
}

void UCodeInterface::PrepareBootUCode(u32 mail)
{
  switch (m_next_ucode_steps)
  {
  case 0:
    m_next_ucode.mram_dest_addr = mail;
    break;
  case 1:
    m_next_ucode.mram_size = mail & 0xffff;
    break;
  case 2:
    m_next_ucode.mram_dram_addr = mail & 0xffff;
    break;
  case 3:
    m_next_ucode.iram_mram_addr = mail;
    break;
  case 4:
    m_next_ucode.iram_size = mail & 0xffff;
    break;
  case 5:
    m_next_ucode.iram_dest = mail & 0xffff;
    break;
  case 6:
    m_next_ucode.iram_startpc = mail & 0xffff;
    break;
  case 7:
    m_next_ucode.dram_mram_addr = mail;
    break;
  case 8:
    m_next_ucode.dram_size = mail & 0xffff;
    break;
  case 9:
    m_next_ucode.dram_dest = mail & 0xffff;
    break;
  }
  m_next_ucode_steps++;

  if (m_next_ucode_steps == 10)
  {
    m_next_ucode_steps = 0;
    m_needs_resume_mail = true;
    m_upload_setup_in_progress = false;

    const u32 ector_crc =
        Common::HashEctor(static_cast<u8*>(HLEMemory_Get_Pointer(m_next_ucode.iram_mram_addr)),
                          m_next_ucode.iram_size);

    if (Config::Get(Config::MAIN_DUMP_UCODE))
    {
      auto& system = Core::System::GetInstance();
      auto& memory = system.GetMemory();
      DSP::DumpDSPCode(memory.GetPointer(m_next_ucode.iram_mram_addr), m_next_ucode.iram_size,
                       ector_crc);
    }

    DEBUG_LOG_FMT(DSPHLE, "PrepareBootUCode {:#010x}", ector_crc);
    DEBUG_LOG_FMT(DSPHLE, "DRAM -> MRAM: src {:04x} dst {:08x} size {:04x}",
                  m_next_ucode.mram_dram_addr, m_next_ucode.mram_dest_addr, m_next_ucode.mram_size);
    DEBUG_LOG_FMT(DSPHLE, "MRAM -> IRAM: src {:08x} dst {:04x} size {:04x} startpc {:04x}",
                  m_next_ucode.iram_mram_addr, m_next_ucode.iram_dest, m_next_ucode.iram_size,
                  m_next_ucode.iram_startpc);
    DEBUG_LOG_FMT(DSPHLE, "MRAM -> DRAM: src {:08x} dst {:04x} size {:04x}",
                  m_next_ucode.dram_mram_addr, m_next_ucode.dram_dest, m_next_ucode.dram_size);

    if (m_next_ucode.mram_size)
    {
      WARN_LOG_FMT(DSPHLE, "Trying to boot new ucode with DRAM download - not implemented");
    }
    if (m_next_ucode.dram_size)
    {
      WARN_LOG_FMT(DSPHLE, "Trying to boot new ucode with DRAM upload - not implemented");
    }

    m_dsphle->SwapUCode(ector_crc);
  }
}

void UCodeInterface::DoStateShared(PointerWrap& p)
{
  p.Do(m_upload_setup_in_progress);
  p.Do(m_next_ucode);
  p.Do(m_next_ucode_steps);
  p.Do(m_needs_resume_mail);
}

std::unique_ptr<UCodeInterface> UCodeFactory(u32 crc, DSPHLE* dsphle, bool wii)
{
  switch (crc)
  {
  case UCODE_ROM:
    INFO_LOG_FMT(DSPHLE, "Switching to ROM ucode");
    return std::make_unique<ROMUCode>(dsphle, crc);

  case UCODE_INIT_AUDIO_SYSTEM:
    INFO_LOG_FMT(DSPHLE, "Switching to INIT ucode");
    return std::make_unique<INITUCode>(dsphle, crc);

  case 0x65d6cc6f:  // CARD
    INFO_LOG_FMT(DSPHLE, "Switching to CARD ucode");
    return std::make_unique<CARDUCode>(dsphle, crc);

  case 0xdd7e72d5:
    INFO_LOG_FMT(DSPHLE, "Switching to GBA ucode");
    return std::make_unique<GBAUCode>(dsphle, crc);

  case 0x3ad3b7ac:  // Naruto 3, Paper Mario - The Thousand Year Door
  case 0x3daf59b9:  // Alien Hominid
  case 0x4e8a8b21:  // spdemo, Crazy Taxi, 18 Wheeler, Disney, Monkeyball 1/2, Cubivore, Nintendo
                    // Puzzle Collection, Wario,
  // Capcom vs. SNK 2, Naruto 2, Lost Kingdoms, Star Fox, Mario Party 4, Mortal Kombat,
  // Smugglers Run Warzone, Smash Brothers, Sonic Mega Collection, ZooCube
  // nddemo, Star Fox
  case 0x07f88145:  // bustamove, Ikaruga, F-Zero GX, Robotech Battle Cry, Star Soldier, Soul
                    // Calibur 2,
                    // Zelda:OOT, Tony Hawk, Viewtiful Joe
  case 0xe2136399:  // Billy Hatcher, Dragon Ball Z, Mario Party 5, TMNT, 1080° Avalanche
  case 0x3389a79e:  // MP1/MP2 Wii (Metroid Prime Trilogy)
    INFO_LOG_FMT(DSPHLE, "CRC {:08x}: AX ucode chosen", crc);
    return std::make_unique<AXUCode>(dsphle, crc);

  case 0x86840740:  // Zelda WW - US
  case 0x6ca33a6d:  // Zelda TP GC - US
  case 0xd643001f:  // Super Mario Galaxy - US
  case 0x6ba3b3ea:  // GC IPL - PAL
  case 0x24b22038:  // GC IPL - NTSC
  case 0x2fcdf1ec:  // Zelda FSA - US
  case 0xdf059f68:  // Pikmin 1 GC - US Demo
  case 0x4be6a5cb:  // Pikmin 1 GC - US
  case 0x267fd05a:  // Pikmin 1 GC - PAL
  case 0x42f64ac4:  // Luigi's Mansion - US
  case 0x56d36052:  // Super Mario Sunshine - US
  case 0x6c3f6f94:  // Zelda TP Wii - US
  case 0xb7eb9a9c:  // Pikmin 1 New Play Control Wii - US
  case 0xeaeb38cc:  // Pikmin 2 New Play Control Wii - US
    return std::make_unique<ZeldaUCode>(dsphle, crc);

  case 0x2ea36ce6:  // Some Wii demos
  case 0x5ef56da3:  // AX demo
  case 0x347112ba:  // Raving Rabbids
  case 0xfa450138:  // Wii Sports - PAL
  case 0xadbc06bd:  // Elebits
  case 0x4cc52064:  // Bleach: Versus Crusade
  case 0xd9c4bf34:  // Wii System Menu 1.0
  case 0x7699af32:  // Wii Startup Menu
    INFO_LOG_FMT(DSPHLE, "CRC {:08x}: Wii - AXWii chosen", crc);
    return std::make_unique<AXWiiUCode>(dsphle, crc);

  case ASndUCode::HASH_2008:
  case ASndUCode::HASH_2009:
  case ASndUCode::HASH_2011:
  case ASndUCode::HASH_2020:
  case ASndUCode::HASH_2020_PAD:
  case ASndUCode::HASH_DESERT_BUS_2011:
  case ASndUCode::HASH_DESERT_BUS_2012:
    INFO_LOG_FMT(DSPHLE, "CRC {:08x}: ASnd chosen (Homebrew)", crc);
    return std::make_unique<ASndUCode>(dsphle, crc);

  case AESndUCode::HASH_2010:
  case AESndUCode::HASH_2012:
  case AESndUCode::HASH_EDUKE32:
  case AESndUCode::HASH_2020:
  case AESndUCode::HASH_2020_PAD:
  case AESndUCode::HASH_2022_PAD:
    INFO_LOG_FMT(DSPHLE, "CRC {:08x}: AESnd chosen (Homebrew)", crc);
    return std::make_unique<AESndUCode>(dsphle, crc);

  default:
    if (wii)
    {
      PanicAlertFmtT(
          "This title might be incompatible with DSP HLE emulation. Try using LLE if this "
          "is homebrew.\n\n"
          "Unknown ucode (CRC = {0:08x}) - forcing AXWii.",
          crc);
      return std::make_unique<AXWiiUCode>(dsphle, crc);
    }
    else
    {
      PanicAlertFmtT(
          "This title might be incompatible with DSP HLE emulation. Try using LLE if this "
          "is homebrew.\n\n"
          "DSPHLE: Unknown ucode (CRC = {0:08x}) - forcing AX.",
          crc);
      return std::make_unique<AXUCode>(dsphle, crc);
    }

  case UCODE_NULL:
    break;
  }

  return nullptr;
}
}  // namespace DSP::HLE
