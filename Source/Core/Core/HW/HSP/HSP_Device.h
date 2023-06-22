// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "Common/CommonTypes.h"

class PointerWrap;

namespace HSP
{
enum class HSPDeviceType : int
{
  None,
  ARAMExpansion,
};

class IHSPDevice
{
public:
  explicit IHSPDevice(HSPDeviceType device_type);
  virtual ~IHSPDevice() = default;

  HSPDeviceType GetDeviceType() const;

  virtual void Write(u32 address, u64 value) = 0;
  virtual u64 Read(u32 address) = 0;

  // Savestate support
  virtual void DoState(PointerWrap& p);

protected:
  HSPDeviceType m_device_type;
};

std::unique_ptr<IHSPDevice> HSPDevice_Create(HSPDeviceType device);

}  // namespace HSP
