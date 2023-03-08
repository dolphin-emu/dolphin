// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/EXI/EXI_DeviceDummy.h"

#include <string>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

namespace ExpansionInterface
{
CEXIDummy::CEXIDummy(Core::System& system, const std::string& name)
    : IEXIDevice(system), m_name{name}
{
}

void CEXIDummy::ImmWrite(u32 data, u32 size)
{
  INFO_LOG_FMT(EXPANSIONINTERFACE, "EXI DUMMY {} ImmWrite: {:08x}", m_name, data);
}

u32 CEXIDummy::ImmRead(u32 size)
{
  INFO_LOG_FMT(EXPANSIONINTERFACE, "EXI DUMMY {} ImmRead", m_name);
  return 0;
}

void CEXIDummy::DMAWrite(u32 address, u32 size)
{
  INFO_LOG_FMT(EXPANSIONINTERFACE, "EXI DUMMY {} DMAWrite: {:08x} bytes, from {:08x} to device",
               m_name, size, address);
}

void CEXIDummy::DMARead(u32 address, u32 size)
{
  INFO_LOG_FMT(EXPANSIONINTERFACE, "EXI DUMMY {} DMARead:  {:08x} bytes, from device to {:08x}",
               m_name, size, address);
}

bool CEXIDummy::IsPresent() const
{
  return true;
}

void CEXIDummy::TransferByte(u8& byte)
{
}
}  // namespace ExpansionInterface
