// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/HSP/HSP_DeviceNull.h"

#include "Core/HW/DSP.h"
#include "Core/HW/HSP/HSP.h"
#include "Core/HW/ProcessorInterface.h"

#include <cstring>

#include "Common/Logging/Log.h"

namespace HSP
{
CHSPDevice_Null::CHSPDevice_Null(HSPDevices device) : IHSPDevice(device)
{
}

u64 CHSPDevice_Null::Read(u32 address)
{
  return 0;
}

void CHSPDevice_Null::Write(u32 address, u64 value)
{
}

}  // namespace HSP
