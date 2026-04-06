// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/HSP/HSP.h"

#include <memory>

#include <fmt/ranges.h>

#include "Common/ChunkFile.h"
#include "Core/Config/MainSettings.h"
#include "Core/HW/HSP/HSP_Device.h"

namespace HSP
{
HSPManager::HSPManager() = default;
HSPManager::~HSPManager() = default;

void HSPManager::Init()
{
  SetDevice(Config::Get(Config::MAIN_HSP_DEVICE));
}

void HSPManager::Shutdown()
{
  m_device.reset();
}

void HSPManager::Read(u32 address, std::span<u8, TRANSFER_SIZE> data)
{
  DEBUG_LOG_FMT(HSP, "HSP read from 0x{:08x}", address);

  m_device->Read(address, data);
}

void HSPManager::Write(u32 address, std::span<const u8, TRANSFER_SIZE> data)
{
  DEBUG_LOG_FMT(HSP, "HSP write to 0x{:08x}: {:02x}", address, fmt::join(data, " "));

  m_device->Write(address, data);
}

void HSPManager::DoState(PointerWrap& p)
{
  const HSPDeviceType current_type = m_device->GetDeviceType();
  auto state_type = current_type;
  p.Do(state_type);

  // If the type doesn't match, switch to the right device type
  if (state_type != current_type)
    SetDevice(state_type);

  m_device->DoState(p);
}

void HSPManager::SetDevice(std::unique_ptr<IHSPDevice> device)
{
  m_device = std::move(device);
}

void HSPManager::SetDevice(const HSPDeviceType device)
{
  SetDevice(HSPDevice_Create(device));
}

}  // namespace HSP
