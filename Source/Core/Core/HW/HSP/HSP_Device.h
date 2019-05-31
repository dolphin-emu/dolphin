// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "Common/CommonTypes.h"

class PointerWrap;

namespace HSP
{
enum HSPDevices : int
{
  HSPDEVICE_NONE,
  HSPDEVICE_ARAM_EXPANSION,
  HSPDEVICE_GB_PLAYER
};

class IHSPDevice
{
public:
  explicit IHSPDevice(HSPDevices device_type);
  virtual ~IHSPDevice() = default;

  HSPDevices GetDeviceType() const;

  virtual void Write(u32 address, u64 value) = 0;
  virtual u64 Read(u32 address) = 0;

  // Savestate support
  virtual void DoState(PointerWrap& p);

protected:
  HSPDevices m_device_type;
};

std::unique_ptr<IHSPDevice> HSPDevice_Create(HSPDevices device);

}  // namespace HSP
