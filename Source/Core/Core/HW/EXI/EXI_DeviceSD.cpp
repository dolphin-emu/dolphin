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
    INFO_LOG_FMT(EXPANSIONINTERFACE, "SD: EXI_GetID detected (size = {:x}, data = {:x})", size, data);
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
    return IEXIDevice::ImmRead(size);
  }
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
  p.Do(command);
  p.Do(m_uPosition);
}

void CEXISD::TransferByte(u8& byte)
{
}
}  // namespace ExpansionInterface
