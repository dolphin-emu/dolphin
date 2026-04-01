// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/HSP/HSP_Device.h"

#include <memory>

#include "Core/HW/HSP/HSP_DeviceARAMExpansion.h"
#include "Core/HW/HSP/HSP_DeviceGBPlayer.h"
#include "Core/HW/HSP/HSP_DeviceNull.h"
#include "Core/System.h"

namespace HSP
{
IHSPDevice::IHSPDevice(HSPDeviceType device_type) : m_device_type(device_type)
{
}

HSPDeviceType IHSPDevice::GetDeviceType() const
{
  return m_device_type;
}

void IHSPDevice::DoState(PointerWrap& p)
{
}

// F A C T O R Y
std::unique_ptr<IHSPDevice> HSPDevice_Create(const HSPDeviceType device)
{
  auto& system = Core::System::GetInstance();

  switch (device)
  {
  case HSPDeviceType::ARAMExpansion:
    return std::make_unique<CHSPDevice_ARAMExpansion>(device);
  case HSPDeviceType::GBPlayer:
    return std::make_unique<CHSPDevice_GBPlayer>(system, device);
  case HSPDeviceType::None:
  default:
    return std::make_unique<CHSPDevice_Null>(device);
  }
}
}  // namespace HSP
