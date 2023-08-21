// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/HSP/HSP.h"

#include <memory>

#include "Common/ChunkFile.h"
#include "Core/Config/MainSettings.h"
#include "Core/HW/HSP/HSP_Device.h"

namespace HSP
{
HSPManager::HSPManager() = default;
HSPManager::~HSPManager() = default;

void HSPManager::Init()
{
  AddDevice(Config::Get(Config::MAIN_HSP_DEVICE));
}

void HSPManager::Shutdown()
{
  RemoveDevice();
}

u64 HSPManager::Read(u32 address)
{
  DEBUG_LOG_FMT(HSP, "HSP read from 0x{:08x}", address);
  if (m_device)
    return m_device->Read(address);
  return 0;
}

void HSPManager::Write(u32 address, u64 value)
{
  DEBUG_LOG_FMT(HSP, "HSP write to 0x{:08x}: 0x{:016x}", address, value);
  if (m_device)
    m_device->Write(address, value);
}

void HSPManager::DoState(PointerWrap& p)
{
  HSPDeviceType type = m_device->GetDeviceType();
  p.Do(type);

  // If the type doesn't match, switch to the right device type
  if (type != m_device->GetDeviceType())
    AddDevice(type);

  m_device->DoState(p);
}

void HSPManager::AddDevice(std::unique_ptr<IHSPDevice> device)
{
  // Set the new one
  m_device = std::move(device);
}

void HSPManager::AddDevice(const HSPDeviceType device)
{
  AddDevice(HSPDevice_Create(device));
}

void HSPManager::RemoveDevice()
{
  m_device.reset();
}
}  // namespace HSP
