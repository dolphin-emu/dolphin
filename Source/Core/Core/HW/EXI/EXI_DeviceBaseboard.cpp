// Copyright 2013 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/EXI/EXI_DeviceBaseboard.h"

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"

namespace ExpansionInterface
{
CEXIBaseboard::CEXIBaseboard()
{
  EEPROM_filename = File::GetUserPath(D_TRIUSER_IDX) + "EEPROM.raw";
  if (File::Exists(EEPROM_filename))
  {
    m_EEPROM = std::make_unique<File::IOFile>(EEPROM_filename, "rb+");
  }
  else
  {
    m_EEPROM = std::make_unique<File::IOFile>(EEPROM_filename, "wb+");
  }
}

CEXIBaseboard::~CEXIBaseboard()
{
  m_EEPROM->Close();
  File::Delete(EEPROM_filename);
}

void CEXIBaseboard::SetCS(int cs)
{
  if (cs)
    m_position = 0;
}

bool CEXIBaseboard::IsPresent() const
{
  return true;
}

void CEXIBaseboard::TransferByte(u8& byte)
{
  if (m_position == 0)
  {
    m_command = byte;
  }
  else
  {
    switch (m_command)
    {
    case 0x00:
    {
      static constexpr std::array<u8, 4> ID = {0x06, 0x04, 0x10, 0x00};
      byte = ID[(m_position - 2) & 3];
      break;
    }
    case 0x01:
      byte = 0x01;
      break;
    case 0x02:
      byte = 0x01;
      break;
    case 0x03:
      byte = 0x01;
      break;
    case 0xFF:
    {
      if ((m_subcommand[1] == 0) && (m_subcommand[2] == 0))
      {
        m_have_irq = true;
        m_irq_timer = 0;
        m_irq_status = 0x02;
      }
      if ((m_subcommand[1] == 2) && (m_subcommand[2] == 1))
      {
        m_irq_status = 0;
      }
      byte = 0x04;
      break;
    }
    default:
      ERROR_LOG_FMT(EXPANSIONINTERFACE, "EXI BASEBOARD: Unhandled command {:#X} {:#X}", m_command,
                    m_position);
      byte = 0x04;
      break;
    }
  }
  m_position++;
}

bool CEXIBaseboard::IsInterruptSet()
{
  if (m_have_irq)
  {
    if (++m_irq_timer > 4)
      m_have_irq = false;
    return 1;
  }
  else
  {
    return 0;
  }
}

void CEXIBaseboard::DoState(PointerWrap& p)
{
  p.Do(m_position);
  p.Do(m_command);
  p.Do(m_subcommand);
  p.Do(m_have_irq);
}
}  // namespace ExpansionInterface
