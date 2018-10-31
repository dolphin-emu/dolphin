// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/EXI/EXI_DeviceAD16.h"

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"

namespace ExpansionInterface
{
CEXIAD16::CEXIAD16() = default;

void CEXIAD16::SetCS(int cs)
{
  if (cs)
    m_position = 0;
}

bool CEXIAD16::IsPresent() const
{
  return true;
}

void CEXIAD16::TransferByte(u8& byte)
{
  if (m_position == 0)
  {
    m_command = byte;
  }
  else
  {
    switch (m_command)
    {
    case init:
    {
      m_ad16_register.U32 = 0x04120000;
      switch (m_position)
      {
      case 1:
        DEBUG_ASSERT(byte == 0x00);
        break;  // just skip
      case 2:
        byte = m_ad16_register.U8[0];
        break;
      case 3:
        byte = m_ad16_register.U8[1];
        break;
      case 4:
        byte = m_ad16_register.U8[2];
        break;
      case 5:
        byte = m_ad16_register.U8[3];
        break;
      }
    }
    break;

    case write:
    {
      switch (m_position)
      {
      case 1:
        m_ad16_register.U8[0] = byte;
        break;
      case 2:
        m_ad16_register.U8[1] = byte;
        break;
      case 3:
        m_ad16_register.U8[2] = byte;
        break;
      case 4:
        m_ad16_register.U8[3] = byte;
        break;
      }
    }
    break;

    case read:
    {
      switch (m_position)
      {
      case 1:
        byte = m_ad16_register.U8[0];
        break;
      case 2:
        byte = m_ad16_register.U8[1];
        break;
      case 3:
        byte = m_ad16_register.U8[2];
        break;
      case 4:
        byte = m_ad16_register.U8[3];
        break;
      }
    }
    break;
    }
  }

  m_position++;
}

void CEXIAD16::DoState(PointerWrap& p)
{
  p.Do(m_position);
  p.Do(m_command);
  p.Do(m_ad16_register);
}
}  // namespace ExpansionInterface
