// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "Common/CommonTypes.h"

class PointerWrap;

namespace SerialInterface
{
// Devices can reply with these
constexpr u32 SI_ERROR_NO_RESPONSE = 0x0008;  // Nothing is attached
constexpr u32 SI_ERROR_UNKNOWN = 0x0040;      // Unknown device is attached
constexpr u32 SI_ERROR_BUSY = 0x0080;         // Still detecting

// Device types
constexpr u32 SI_TYPE_MASK = 0x18000000;  // ???
constexpr u32 SI_TYPE_GC = 0x08000000;

// GC Controller types
constexpr u32 SI_GC_NOMOTOR = 0x20000000;  // No rumble motor
constexpr u32 SI_GC_STANDARD = 0x01000000;

// SI Device IDs for emulator use
enum TSIDevices : u32
{
  SI_NONE = SI_ERROR_NO_RESPONSE,
  SI_N64_MIC = 0x00010000,
  SI_N64_KEYBOARD = 0x00020000,
  SI_N64_MOUSE = 0x02000000,
  SI_N64_CONTROLLER = 0x05000000,
  SI_GBA = 0x00040000,
  SI_GC_CONTROLLER = (SI_TYPE_GC | SI_GC_STANDARD),
  SI_GC_KEYBOARD = (SI_TYPE_GC | 0x00200000),
  SI_GC_STEERING =
      SI_TYPE_GC,  // (shuffle2)I think the "chainsaw" is the same (Or else it's just standard)
  SI_DANCEMAT = (SI_TYPE_GC | SI_GC_STANDARD | 0x00000300),
  SI_AM_BASEBOARD = 0x10110800  // gets ORd with dipswitch state
};

// For configuration use, since some devices can have the same SI Device ID
enum SIDevices : int
{
  SIDEVICE_NONE,
  SIDEVICE_N64_MIC,
  SIDEVICE_N64_KEYBOARD,
  SIDEVICE_N64_MOUSE,
  SIDEVICE_N64_CONTROLLER,
  SIDEVICE_GC_GBA,
  SIDEVICE_GC_CONTROLLER,
  SIDEVICE_GC_KEYBOARD,
  SIDEVICE_GC_STEERING,
  SIDEVICE_DANCEMAT,
  SIDEVICE_GC_TARUKONGA,
  // Was used for Triforce in the past, but the implementation is no longer in Dolphin.
  // It's kept here so that values below will stay constant.
  SIDEVICE_AM_BASEBOARD,
  SIDEVICE_WIIU_ADAPTER,
  // Not a valid device. Used for checking whether enum values are valid.
  SIDEVICE_COUNT,
};

class ISIDevice
{
public:
  ISIDevice(SIDevices device_type, int device_number);
  virtual ~ISIDevice();

  int GetDeviceNumber() const;
  SIDevices GetDeviceType() const;

  // Run the SI Buffer
  virtual int RunBuffer(u8* buffer, int length);
  virtual int TransferInterval();

  // Return true on new data
  virtual bool GetData(u32& hi, u32& low) = 0;

  // Send a command directly (no detour per buffer)
  virtual void SendCommand(u32 command, u8 poll) = 0;

  // Savestate support
  virtual void DoState(PointerWrap& p);

protected:
  int m_device_number;
  SIDevices m_device_type;
};

bool SIDevice_IsGCController(SIDevices type);

std::unique_ptr<ISIDevice> SIDevice_Create(SIDevices device, int port_number);
}  // namespace SerialInterface
