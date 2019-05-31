// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/HSP/HSP_Device.h"

#include <memory>

#include "Common/CommonTypes.h"
#include "Core/HW/HSP/HSP_DeviceARAMExpansion.h"
#include "Core/HW/HSP/HSP_DeviceGBPlayer.h"
#include "Core/HW/HSP/HSP_DeviceNull.h"

namespace HSP
{
IHSPDevice::IHSPDevice(HSPDevices device_type) : m_device_type(device_type)
{
}

HSPDevices IHSPDevice::GetDeviceType() const
{
  return m_device_type;
}

void IHSPDevice::DoState(PointerWrap& p)
{
}

// F A C T O R Y
std::unique_ptr<IHSPDevice> HSPDevice_Create(const HSPDevices device)
{
  switch (device)
  {
  case HSPDEVICE_ARAM_EXPANSION:
    return std::make_unique<CHSPDevice_ARAMExpansion>(device);
  case HSPDEVICE_GB_PLAYER:
    return std::make_unique<CHSPDevice_GBPlayer>(device);
  case HSPDEVICE_NONE:
  default:
    return std::make_unique<CHSPDevice_Null>(device);
  }
}
}  // namespace HSP
