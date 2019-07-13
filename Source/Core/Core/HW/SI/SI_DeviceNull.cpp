// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/SI/SI_DeviceNull.h"
#include "Common/Swap.h"

#include <cstring>

namespace SerialInterface
{
CSIDevice_Null::CSIDevice_Null(SIDevices device, int device_number)
    : ISIDevice{device, device_number}
{
}

int CSIDevice_Null::RunBuffer(u8* buffer, int request_length)
{
  return 0;
}

bool CSIDevice_Null::GetData(GCPadStatus& pad_status, u32& hi, u32& low)
{
  hi = 0x80000000;
  return true;
}

void CSIDevice_Null::SendCommand(u32 command, u8 poll)
{
}
}  // namespace SerialInterface
