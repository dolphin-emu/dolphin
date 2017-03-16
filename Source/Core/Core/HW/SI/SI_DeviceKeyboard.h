// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/HW/SI/SI_Device.h"

class PointerWrap;
struct KeyboardStatus;

namespace SerialInterface
{
class CSIDevice_Keyboard : public ISIDevice
{
public:
  // Constructor
  CSIDevice_Keyboard(SIDevices device, int device_number);

  // Run the SI Buffer
  int RunBuffer(u8* buffer, int length) override;

  // Return true on new data
  bool GetData(u32& hi, u32& low) override;

  KeyboardStatus GetKeyboardStatus() const;
  void MapKeys(const KeyboardStatus& key_status, u8* key);

  // Send a command directly
  void SendCommand(u32 command, u8 poll) override;

  // Savestate support
  void DoState(PointerWrap& p) override;

protected:
  // Commands
  enum EBufferCommands
  {
    CMD_RESET = 0x00,
    CMD_DIRECT = 0x54,
    CMD_ID = 0xff,
  };

  enum EDirectCommands
  {
    CMD_WRITE = 0x40,
    CMD_POLL = 0x54
  };

  union UCommand
  {
    u32 hex = 0;
    struct
    {
      u32 parameter1 : 8;
      u32 parameter2 : 8;
      u32 command : 8;
      u32 : 8;
    };
    UCommand() = default;
    UCommand(u32 value) : hex{value} {}
  };

  // PADAnalogMode
  u8 m_mode = 0;

  // Internal counter synchonizing GC and keyboard
  u8 m_counter = 0;
};
}  // namespace SerialInterface
