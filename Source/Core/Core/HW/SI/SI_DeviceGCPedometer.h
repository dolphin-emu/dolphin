// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/HW/SI/SI_Device.h"

namespace SerialInterface
{
class CSIDevice_GCPedometer : public ISIDevice
{
public:
  // Constructor
  CSIDevice_GCPedometer(SIDevices device, int device_number);

  // Run the SI Buffer
  int RunBuffer(u8* buffer, int length) override;

  // Return true on new data
  bool GetData(u32& hi, u32& low) override;

  // Send a command directly
  void SendCommand(u32 command, u8 poll) override;

  // Savestate support
  void DoState(PointerWrap& p) override;

protected:
  // Commands
  enum EBufferCommands
  {
    CMD_RESET = 0x00,
    CMD_DATA = 0x60
  };

  struct SData
  {
    u8 name_high[3];
    u8 name_low[3];
    u8 gender;
    u8 height;
    u8 weight;
    u8 step_length;
    u8 total_steps[3];
    u8 total_meters[3];
  };

  enum EDirectCommands
  {
    CMD_WRITE = 0x40
  };

  enum EDirectCommandResponses
  {
    DATA_ACKNOWLEDGE_HI = 0x80000000
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

  // Structure for data stored on pedometer
  SData m_data{};

  // Acknowledgement flag for buffer data command
  bool m_acknowledge = false;

  // Flag for the direct reset command
  bool m_reset = false;
};
}  // namespace SerialInterface
