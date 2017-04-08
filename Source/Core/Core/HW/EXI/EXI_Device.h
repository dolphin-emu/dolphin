// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "Common/CommonTypes.h"

class PointerWrap;

namespace ExpansionInterface
{
enum TEXIDevices : int
{
  EXIDEVICE_DUMMY,
  EXIDEVICE_MEMORYCARD,
  EXIDEVICE_MASKROM,
  EXIDEVICE_AD16,
  EXIDEVICE_MIC,
  EXIDEVICE_ETH,
  // Was used for Triforce in the past, but the implementation is no longer in Dolphin.
  // It's kept here so that values below will stay constant.
  EXIDEVICE_AM_BASEBOARD,
  EXIDEVICE_GECKO,
  // Only used when creating a device by EXIDevice_Create.
  // Converted to EXIDEVICE_MEMORYCARD internally.
  EXIDEVICE_MEMORYCARDFOLDER,
  EXIDEVICE_AGP,
  EXIDEVICE_NONE = 0xFF
};

class IEXIDevice
{
public:
  virtual ~IEXIDevice() = default;

  // Immediate copy functions
  virtual void ImmWrite(u32 data, u32 size);
  virtual u32 ImmRead(u32 size);
  virtual void ImmReadWrite(u32& data, u32 size);

  // DMA copy functions
  virtual void DMAWrite(u32 address, u32 size);
  virtual void DMARead(u32 address, u32 size);

  virtual IEXIDevice* FindDevice(TEXIDevices device_type, int custom_index = -1);

  virtual bool UseDelayedTransferCompletion() const;
  virtual bool IsPresent() const;
  virtual void SetCS(int cs);
  virtual void DoState(PointerWrap& p);
  virtual void PauseAndLock(bool do_lock, bool resume_on_unlock = true);

  // Is generating interrupt ?
  virtual bool IsInterruptSet();

  // For savestates. storing it here seemed cleaner than requiring each implementation to report its
  // type. I know this class is set up like an interface, but no code requires it to be strictly
  // such.
  TEXIDevices m_device_type;

private:
  // Byte transfer function for this device
  virtual void TransferByte(u8& byte);
};

std::unique_ptr<IEXIDevice> EXIDevice_Create(TEXIDevices device_type, int channel_num);
}  // namespace ExpansionInterface
