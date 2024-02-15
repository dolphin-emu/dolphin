// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/SI/SI_DeviceNull.h"

namespace SerialInterface
{
CSIDevice_Null::CSIDevice_Null(Core::System& system, SIDevices device, int device_number)
    : ISIDevice{system, device, device_number}
{
}

int CSIDevice_Null::RunBuffer(u8* buffer, int request_length)
{
  return -1;
}

bool CSIDevice_Null::GetData(u32& hi, u32& low)
{
  hi = 0x80000000;
  return true;
}

void CSIDevice_Null::SendCommand(u32 command, u8 poll)
{
}
}  // namespace SerialInterface
