// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/EXI/EXI_DeviceSD.h"

#include <string>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Hash.h"
#include "Common/Logging/Log.h"

namespace ExpansionInterface
{
CEXISD::CEXISD(Core::System& system) : IEXIDevice(system)
{
}

void CEXISD::ImmWrite(u32 data, u32 size)
{
  if (inited)
  {
    while (size--)
    {
      u8 byte = data >> 24;
      WriteByte(byte);
      data <<= 8;
    }
  }
  else if (size == 2 && data == 0)
  {
    // Get ID command
    INFO_LOG_FMT(EXPANSIONINTERFACE, "SD: EXI_GetID detected (size = {:x}, data = {:x})", size,
                 data);
    get_id = true;
  }
}

u32 CEXISD::ImmRead(u32 size)
{
  if (get_id)
  {
    // This is not a good way of handling state
    inited = true;
    get_id = false;
    INFO_LOG_FMT(EXPANSIONINTERFACE, "SD: EXI_GetID finished (size = {:x})", size);
    // Same signed/unsigned mismatch in libogc; it wants -1
    return -1;
  }
  else
  {
    u32 res = 0;
    u32 position = 0;
    while (size--)
    {
      u8 byte = ReadByte();
      res |= byte << (24 - (position++ * 8));
    }
    INFO_LOG_FMT(EXPANSIONINTERFACE, "Responding with {:08x}", res);
    return res;
  }
}

void CEXISD::ImmReadWrite(u32& data, u32 size)
{
  ImmWrite(data, size);
  data = ImmRead(size);
}

void CEXISD::SetCS(int cs)
{
  INFO_LOG_FMT(EXPANSIONINTERFACE, "EXI SD SetCS: {}", cs);
}

bool CEXISD::IsPresent() const
{
  return true;
}

void CEXISD::DoState(PointerWrap& p)
{
  p.Do(inited);
  p.Do(get_id);
  p.Do(m_uPosition);
  p.DoArray(cmd);
  p.Do(response);
}

void CEXISD::WriteByte(u8 byte)
{
  // TODO: Write-protect inversion(?)
  if (m_uPosition == 0)
  {
    if ((byte & 0b11000000) == 0b01000000)
    {
      INFO_LOG_FMT(EXPANSIONINTERFACE, "EXI SD command started: {:02x}", byte);
      cmd[m_uPosition++] = byte;
    }
  }
  else if (m_uPosition < 6)
  {
    cmd[m_uPosition++] = byte;

    if (m_uPosition == 6)
    {
      // Buffer now full
      m_uPosition = 0;

      if ((byte & 1) != 1)
      {
        INFO_LOG_FMT(EXPANSIONINTERFACE, "EXI SD command invalid, last bit not set: {:02x}", byte);
        return;
      }

      // TODO: Check CRC

      INFO_LOG_FMT(EXPANSIONINTERFACE, "EXI SD command received: {:02x}", fmt::join(cmd.begin(), cmd.end(), " "));

      if (cmd[0] == 0x40)  // GO_IDLE_STATE
      {
        response.push_back(static_cast<u8>(R1::InIdleState));
      }
      else if (cmd[0] == 0x41)  // SEND_OP_COND
      {
        // R1
        response.push_back(0);
      }
      else if (cmd[0] == 0x48)  // SEND_IF_COND
      {
        // Format R7
        u8 supply_voltage = cmd[4] & 0xF;
        u8 check_pattern = cmd[5];
        response.push_back(static_cast<u8>(R1::InIdleState));  // R1
        response.push_back(0);               // Command version nybble (0), reserved
        response.push_back(0);               // Reserved
        response.push_back(supply_voltage);  // Reserved + voltage
        response.push_back(check_pattern);
      }
      else if (cmd[0] == 0x49 || cmd[0] == 0x4A)  // SEND_CSD or SEND_CID
      {
        // R1
        response.push_back(0);
        // Data ready token
        response.push_back(0xfe);
        // CSD or CID - 16 bytes, including a CRC7 in the last byte and a reserved LSB
        // However, nothing seems to check the CRC7 currently
        for (size_t i = 0; i < 16; i++)
          response.push_back(0);
        // CRC16 of all 0 is 0, but we'll want to implement it properly later
        response.push_back(0);
        response.push_back(0);
      }
      else
      {
        // Don't know it
        response.push_back(static_cast<u8>(R1::IllegalCommand));
      }
    }
  }
}

u8 CEXISD::ReadByte()
{
  if (response.empty())
  {
    WARN_LOG_FMT(EXPANSIONINTERFACE, "Attempted to read from empty SD queue");
    return 0xFF;
  }
  else
  {
    u8 result = response.front();
    response.pop_front();
    return result;
  }
}
}  // namespace ExpansionInterface
