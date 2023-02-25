// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/HSP/HSP.h"

#include <memory>

#include "Common/ChunkFile.h"
#include "Core/Config/MainSettings.h"
#include "Core/HW/HSP/HSP_Device.h"

namespace HSP
{
static std::unique_ptr<IHSPDevice> s_device;

void Init()
{
  AddDevice(Config::Get(Config::MAIN_HSP_DEVICE));
}

void Shutdown()
{
  RemoveDevice();
}

u64 Read(u32 address)
{
  DEBUG_LOG_FMT(HSP, "HSP read from 0x{:08x}", address);
  if (s_device)
    return s_device->Read(address);
  return 0;
}

void Write(u32 address, u64 value)
{
  DEBUG_LOG_FMT(HSP, "HSP write to 0x{:08x}: 0x{:016x}", address, value);
  if (s_device)
    s_device->Write(address, value);
}

void DoState(PointerWrap& p)
{
  HSPDeviceType type = s_device->GetDeviceType();
  p.Do(type);

  // If the type doesn't match, switch to the right device type
  if (type != s_device->GetDeviceType())
    AddDevice(type);

  s_device->DoState(p);
}

void AddDevice(std::unique_ptr<IHSPDevice> device)
{
  // Set the new one
  s_device = std::move(device);
}

void AddDevice(const HSPDeviceType device)
{
  AddDevice(HSPDevice_Create(device));
}

void RemoveDevice()
{
  s_device.reset();
}
}  // namespace HSP
