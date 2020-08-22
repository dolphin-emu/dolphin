// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/EXI/EXI_DeviceSD.h"

#include <string>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
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
    IEXIDevice::ImmWrite(data, size);
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
    u32 res = IEXIDevice::ImmRead(size);
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
  p.Do(result);
}

void CEXISD::TransferByte(u8& byte)
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

      result = R1::InIdleState;
    }
  }

  byte = static_cast<u8>(result);
}
}  // namespace ExpansionInterface
