// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include "Common/CommonTypes.h"

class PointerWrap;

namespace Memcard
{
struct HeaderData;
}

namespace ExpansionInterface
{
enum class EXIDeviceType : int
{
  Dummy,
  MemoryCard,
  MaskROM,
  AD16,
  Microphone,
  Ethernet,
  // Was used for Triforce in the past, but the implementation is no longer in Dolphin.
  // It's kept here so that values below will stay constant.
  AMBaseboard,
  Gecko,
  // Only used when creating a device by EXIDevice_Create.
  // Converted to MemoryCard internally.
  MemoryCardFolder,
  AGP,
  EthernetXLink,
  // Only used on Apple devices.
  EthernetTapServer,
  None = 0xFF
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

  virtual IEXIDevice* FindDevice(EXIDeviceType device_type, int custom_index = -1);

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
  EXIDeviceType m_device_type = EXIDeviceType::None;

private:
  // Byte transfer function for this device
  virtual void TransferByte(u8& byte);
};

std::unique_ptr<IEXIDevice> EXIDevice_Create(EXIDeviceType device_type, int channel_num,
                                             const Memcard::HeaderData& memcard_header_data);
}  // namespace ExpansionInterface
