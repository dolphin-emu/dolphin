// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <span>

#include "Common/CommonTypes.h"

class PointerWrap;

namespace HSP
{

static constexpr std::size_t TRANSFER_SIZE = 32;

enum class HSPDeviceType : int
{
  None,
  ARAMExpansion,
  GBPlayer,
};

class IHSPDevice
{
public:
  virtual ~IHSPDevice();

  virtual HSPDeviceType GetDeviceType() const = 0;

  // Note: DSPManager::RegisterMMIO ensures addresses are 32 byte aligned.

  virtual void Read(u32 address, std::span<u8, TRANSFER_SIZE> data) = 0;
  virtual void Write(u32 address, std::span<const u8, TRANSFER_SIZE> data) = 0;

  // Savestate support
  virtual void DoState(PointerWrap& p);
};

std::unique_ptr<IHSPDevice> HSPDevice_Create(HSPDeviceType device);

}  // namespace HSP
