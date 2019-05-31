// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/HSP/HSP.h"

#include <memory>

#include "Common/Logging/Log.h"
#include "Core/ConfigManager.h"
#include "Core/HW/HSP/HSP_Device.h"

namespace HSP
{
static std::unique_ptr<IHSPDevice> s_device;

void Init()
{
  AddDevice(SConfig::GetInstance().m_HSPDevice);
}

void Shutdown()
{
  RemoveDevice();
}

u64 Read(u32 address)
{
  DEBUG_LOG(HSP, "HSP read from 0x%08x", address);
  if (s_device)
    return s_device->Read(address);
  return 0;
}

void Write(u32 address, u64 value)
{
  DEBUG_LOG(HSP, "HSP write to 0x%08x: 0x%016lx", address, value);
  if (s_device)
    s_device->Write(address, value);
}

void DoState(PointerWrap& p)
{
  if (s_device)
    s_device->DoState(p);
}

void AddDevice(std::unique_ptr<IHSPDevice> device)
{
  // Delete the old device
  RemoveDevice();

  // Set the new one
  s_device = std::move(device);
}

void AddDevice(const HSPDevices device)
{
  AddDevice(HSPDevice_Create(device));
}

void RemoveDevice()
{
  s_device.reset();
}
}  // namespace HSP
