// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/EXI/EXI_DeviceBaseboard.h"

#include <numeric>
#include <string>

#include <fmt/format.h>

#include "Common/Buffer.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"

#include "Core/Boot/Boot.h"
#include "Core/BootManager.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/DVD/AMMediaboard.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/EXI/EXI.h"
#include "Core/HW/EXI/EXI_Device.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "Core/Movie.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

static bool s_interrupt_set = false;
static u32 s_irq_timer = 0;
static u32 s_irq_status = 0;

static u16 CheckSum(const u8* data, u32 length)
{
  return std::accumulate(data, data + length, 0);
}

namespace ExpansionInterface
{

void GenerateInterrupt(int flag)
{
  auto& system = Core::System::GetInstance();

  s_interrupt_set = true;
  s_irq_timer = 0;
  s_irq_status = flag;

  system.GetExpansionInterface().UpdateInterrupts();
}

CEXIBaseboard::CEXIBaseboard(Core::System& system) : IEXIDevice(system)
{
  std::string backup_filename(fmt::format("{}tribackup_{}.bin", File::GetUserPath(D_TRIUSER_IDX),
                                          SConfig::GetInstance().GetGameID()));

  m_backup = File::IOFile(backup_filename, "rb+");
  if (!m_backup.IsOpen())
  {
    m_backup = File::IOFile(backup_filename, "wb+");
  }

  // Some games share the same ID Client/Server
  if (!m_backup.IsGood())
  {
    PanicAlertFmt("Failed to open {}\nFile might be in use.", backup_filename.c_str());

    std::srand(static_cast<u32>(std::time(nullptr)));

    backup_filename = fmt::format("{}tribackup_tmp_{}{}.bin", File::GetUserPath(D_TRIUSER_IDX),
                                  rand(), SConfig::GetInstance().GetGameID());

    m_backup = File::IOFile(backup_filename, "wb+");
  }

  // Virtua Striker 4 and Gekitou Pro Yakyuu need a higher FIRM version
  // Which is read from the backup data?!
  if (AMMediaboard::GetGameType() == VirtuaStriker4 ||
      AMMediaboard::GetGameType() == GekitouProYakyuu)
  {
    if (m_backup.GetSize())
    {
      Common::UniqueBuffer<u8> data(m_backup.GetSize());
      m_backup.ReadBytes(data.data(), data.size());

      // Set FIRM version
      Common::BitCastPtr<u16>(data.data() + 0x12) = 0x1703;
      Common::BitCastPtr<u16>(data.data() + 0x212) = 0x1703;

      // Update checksum
      Common::BitCastPtr<u16>(data.data() + 0x0A) = Common::swap16(CheckSum(&data[0xC], 0x1F4));
      Common::BitCastPtr<u16>(data.data() + 0x20A) = Common::swap16(CheckSum(&data[0x20C], 0x1F4));

      m_backup.Seek(0, File::SeekOrigin::Begin);
      m_backup.WriteBytes(data.data(), data.size());
      m_backup.Flush();
    }
  }
}

CEXIBaseboard::~CEXIBaseboard()
{
}

void CEXIBaseboard::SetCS(int cs)
{
  DEBUG_LOG_FMT(SP1, "AM-BB: ChipSelect={}", cs);
  if (cs)
    m_position = 0;
}

bool CEXIBaseboard::IsPresent() const
{
  return true;
}

bool CEXIBaseboard::IsInterruptSet()
{
  if (s_interrupt_set)
  {
    DEBUG_LOG_FMT(SP1, "AM-BB: IRQ");
    if (++s_irq_timer > 12)
      s_interrupt_set = false;
    return true;
  }

  return false;
}

void CEXIBaseboard::DMAWrite(u32 addr, u32 size)
{
  const auto& system = Core::System::GetInstance();
  const auto& memory = system.GetMemory();

  NOTICE_LOG_FMT(SP1, "AM-BB: COMMAND: Backup DMA Write: {:08x} {:x}", addr, size);

  m_backup.Seek(m_backup_offset, File::SeekOrigin::Begin);

  m_backup.WriteBytes(memory.GetSpanForAddress(addr).data(), size);

  m_backup.Flush();
}

void CEXIBaseboard::DMARead(u32 addr, u32 size)
{
  const auto& system = Core::System::GetInstance();
  const auto& memory = system.GetMemory();

  NOTICE_LOG_FMT(SP1, "AM-BB: COMMAND: Backup DMA Read: {:08x} {:x}", addr, size);

  m_backup.Seek(m_backup_offset, File::SeekOrigin::Begin);

  m_backup.Flush();

  m_backup.ReadBytes(memory.GetSpanForAddress(addr).data(), size);
}

void CEXIBaseboard::TransferByte(u8& byte)
{
  DEBUG_LOG_FMT(SP1, "AM-BB: > {:02x}", byte);
  if (m_position < 4)
  {
    m_command[m_position] = byte;
    byte = 0xFF;
  }

  if ((m_position >= 2) && (m_command[0] == 0 && m_command[1] == 0))
  {
    // Read serial ID
    byte = "\x06\x04\x10\x00"[(m_position - 2) & 3];
  }
  else if (m_position == 3)
  {
    u32 checksum = (m_command[0] << 24) | (m_command[1] << 16) | (m_command[2] << 8);
    u32 bit = 0x80000000UL;
    u32 check = 0x8D800000UL;
    while (bit >= 0x100)
    {
      if (checksum & bit)
        checksum ^= check;
      check >>= 1;
      bit >>= 1;
    }

    if (m_command[3] != (checksum & 0xFF))
      DEBUG_LOG_FMT(SP1, "AM-BB: cs: {:02x}, w: {:02x}", m_command[3], checksum & 0xFF);
  }
  else if (m_position == 4)
  {
    switch (m_command[0])
    {
    case BackupOffsetSet:
      m_backup_offset = (m_command[1] << 8) | m_command[2];
      DEBUG_LOG_FMT(SP1, "AM-BB: COMMAND: BackupOffsetSet:{:04x}", m_backup_offset);
      m_backup.Seek(m_backup_offset, File::SeekOrigin::Begin);
      byte = 0x01;
      break;
    case BackupWrite:
      DEBUG_LOG_FMT(SP1, "AM-BB: COMMAND: BackupWrite:{:04x}-{:02x}", m_backup_offset,
                    m_command[1]);
      m_backup.WriteBytes(&m_command[1], 1);
      m_backup.Flush();
      byte = 0x01;
      break;
    case BackupRead:
      DEBUG_LOG_FMT(SP1, "AM-BB: COMMAND: BackupRead :{:04x}", m_backup_offset);
      byte = 0x01;
      break;
    case DMAOffsetLengthSet:
      m_backup_dma_offset = (m_command[1] << 8) | m_command[2];
      m_backup_dma_length = m_command[3];
      NOTICE_LOG_FMT(SP1, "AM-BB: COMMAND: DMAOffsetLengthSet :{:04x} {:02x}", m_backup_dma_offset,
                     m_backup_dma_length);
      byte = 0x01;
      break;
    case ReadISR:
      NOTICE_LOG_FMT(SP1, "AM-BB: COMMAND: ReadISR  :{:02x} {:02x}:{:02x} {:02x}", m_command[1],
                     m_command[2], 4, s_irq_status);
      byte = 0x04;
      break;
    case WriteISR:
      NOTICE_LOG_FMT(SP1, "AM-BB: COMMAND: WriteISR :{:02x} {:02x}", m_command[1], m_command[2]);
      s_irq_status &= ~(m_command[2]);
      byte = 0x04;
      break;
    // 2 byte out
    case ReadIMR:
      NOTICE_LOG_FMT(SP1, "AM-BB: COMMAND: ReadIMR  :{:02x} {:02x}", m_command[1], m_command[2]);
      byte = 0x04;
      break;
    case WriteIMR:
      NOTICE_LOG_FMT(SP1, "AM-BB: COMMAND: WriteIMR :{:02x} {:02x}", m_command[1], m_command[2]);
      byte = 0x04;
      break;
    case WriteLANCNT:
      NOTICE_LOG_FMT(SP1, "AM-BB: COMMAND: WriteLANCNT :{:02x} {:02x}", m_command[1], m_command[2]);
      if ((m_command[1] == 0) && (m_command[2] == 0))
      {
        s_interrupt_set = true;
        s_irq_timer = 0;
        s_irq_status = 0x02;
      }
      if ((m_command[1] == 2) && (m_command[2] == 1))
      {
        s_irq_status = 0;
      }
      byte = 0x08;
      break;
    default:
      byte = 4;
      ERROR_LOG_FMT(SP1, "AM-BB: COMMAND: {:02x} {:02x} {:02x}", m_command[0], m_command[1],
                    m_command[2]);
      break;
    }
  }
  else if (m_position > 4)
  {
    switch (m_command[0])
    {
    // 1 byte out
    case BackupRead:
      m_backup.Flush();
      m_backup.ReadBytes(&byte, 1);
      break;
    case DMAOffsetLengthSet:
      byte = 0x01;
      break;
    // 2 byte out
    case ReadISR:
      if (m_position == 6)
      {
        byte = s_irq_status;
        s_interrupt_set = false;
      }
      else
      {
        byte = 0x04;
      }
      break;
    // 2 byte out
    case ReadIMR:
      if (m_position == 5)
        byte = 0xFF;
      if (m_position == 6)
        byte = 0x81;
      break;
    default:
      ERROR_LOG_FMT(SP1, "Unknown AM-BB command: {:02x}", m_command[0]);
      break;
    }
  }
  else
  {
    byte = 0xFF;
  }

  DEBUG_LOG_FMT(SP1, "AM-BB < {:02x}", byte);
  m_position++;
}

void CEXIBaseboard::DoState(PointerWrap& p)
{
  p.Do(m_position);
  p.Do(s_interrupt_set);
  p.Do(m_command);
}

}  // namespace ExpansionInterface
