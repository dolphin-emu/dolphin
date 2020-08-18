// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/EXI/EXI_DeviceSD.h"

#include <string>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

namespace ExpansionInterface
{
CEXISD::CEXISD(Core::System& system) : IEXIDevice(system)
{
}

void CEXISD::ImmWrite(u32 data, u32 size)
{
  INFO_LOG_FMT(EXPANSIONINTERFACE, "EXI SD ImmWrite: {:08x}", data);
}

u32 CEXISD::ImmRead(u32 size)
{
  INFO_LOG_FMT(EXPANSIONINTERFACE, "EXI SD ImmRead");
  return -1;
}

void CEXISD::DMAWrite(u32 address, u32 size)
{
  INFO_LOG_FMT(EXPANSIONINTERFACE, "EXI SD DMAWrite: {:08x} bytes, from {:08x} to device", size,
               address);
}

void CEXISD::DMARead(u32 address, u32 size)
{
  INFO_LOG_FMT(EXPANSIONINTERFACE, "EXI SD DMARead: {:08x} bytes, from device to {:08x}", size,
               address);
}

void CEXISD::SetCS(int cs)
{
  INFO_LOG_FMT(EXPANSIONINTERFACE, "EXI SD SetCS: {}", cs);
}

bool CEXISD::IsPresent() const
{
  return true;
}

void CEXISD::TransferByte(u8& byte)
{
}
}  // namespace ExpansionInterface
