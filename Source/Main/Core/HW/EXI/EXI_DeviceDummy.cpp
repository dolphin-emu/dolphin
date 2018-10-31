// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/EXI/EXI_DeviceDummy.h"

#include <string>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

namespace ExpansionInterface
{
CEXIDummy::CEXIDummy(const std::string& name) : m_name{name}
{
}

void CEXIDummy::ImmWrite(u32 data, u32 size)
{
  INFO_LOG(EXPANSIONINTERFACE, "EXI DUMMY %s ImmWrite: %08x", m_name.c_str(), data);
}

u32 CEXIDummy::ImmRead(u32 size)
{
  INFO_LOG(EXPANSIONINTERFACE, "EXI DUMMY %s ImmRead", m_name.c_str());
  return 0;
}

void CEXIDummy::DMAWrite(u32 address, u32 size)
{
  INFO_LOG(EXPANSIONINTERFACE, "EXI DUMMY %s DMAWrite: %08x bytes, from %08x to device",
           m_name.c_str(), size, address);
}

void CEXIDummy::DMARead(u32 address, u32 size)
{
  INFO_LOG(EXPANSIONINTERFACE, "EXI DUMMY %s DMARead:  %08x bytes, from device to %08x",
           m_name.c_str(), size, address);
}

bool CEXIDummy::IsPresent() const
{
  return true;
}

void CEXIDummy::TransferByte(u8& byte)
{
}
}  // namespace ExpansionInterface
