// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/HSP/HSP_DeviceNull.h"

#include <cstring>

#include "Core/HW/HSP/HSP.h"

namespace HSP
{
CHSPDevice_Null::CHSPDevice_Null(HSPDeviceType device) : IHSPDevice(device)
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
