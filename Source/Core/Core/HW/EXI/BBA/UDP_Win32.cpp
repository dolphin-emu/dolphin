// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/EXI/EXI_Device.h"
#include "Core/HW/EXI/EXI_DeviceEthernet.h"

namespace ExpansionInterface
{
bool CEXIETHERNET::UDPPhysicalNetworkInterface::Activate()
{
  return false;
}

void CEXIETHERNET::UDPPhysicalNetworkInterface::Deactivate()
{
}

bool CEXIETHERNET::UDPPhysicalNetworkInterface::IsActivated()
{
  return false;
}

bool CEXIETHERNET::UDPPhysicalNetworkInterface::SendFrame(const u8* frame, u32 size)
{
  return false;
}

void CEXIETHERNET::UDPPhysicalNetworkInterface::ReadThreadHandler(
    CEXIETHERNET::UDPPhysicalNetworkInterface* self)
{
}

bool CEXIETHERNET::UDPPhysicalNetworkInterface::RecvInit()
{
  return true;
}

void CEXIETHERNET::UDPPhysicalNetworkInterface::RecvStart()
{
}

void CEXIETHERNET::UDPPhysicalNetworkInterface::RecvStop()
{
}
}  // namespace ExpansionInterface
