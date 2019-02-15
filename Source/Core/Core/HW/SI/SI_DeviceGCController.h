// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/HW/GCPad.h"
#include "Core/HW/SI/SI_Device.h"
#include "InputCommon/GCPadStatus.h"

namespace SerialInterface
{
class CSIDevice_GCController : public ISIDevice
{
protected:
  // Commands
  enum EBufferCommands
  {
    CMD_RESET = 0x00,
    CMD_DIRECT = 0x40,
    CMD_ORIGIN = 0x41,
    CMD_RECALIBRATE = 0x42,
    CMD_ID = 0xff,
  };

  struct SOrigin
  {
    u16 button;
    u8 origin_stick_x;
    u8 origin_stick_y;
    u8 substick_x;
    u8 substick_y;
    u8 trigger_left;
    u8 trigger_right;
    u8 unk_4;
    u8 unk_5;
    u8 unk_6;
    u8 unk_7;
  };

  enum EDirectCommands
  {
    CMD_WRITE = 0x40
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

  enum EButtonCombo
  {
    COMBO_NONE = 0,
    COMBO_ORIGIN,
    COMBO_RESET
  };

  // struct to compare input against
  // Set on connection and (standard pad only) on button combo
  SOrigin m_origin;

  bool m_calibrated = false;

  // PADAnalogMode
  // Dunno if we need to do this, game/lib should set it?
  u8 m_mode = 0x3;

  // Timer to track special button combos:
  // y, X, start for 3 seconds updates origin with current status
  //   Technically, the above is only on standard pad, wavebird does not support it for example
  // b, x, start for 3 seconds triggers reset (PI reset button interrupt)
  u64 m_timer_button_combo_start = 0;
  u64 m_timer_button_combo = 0;
  // Type of button combo from the last/current poll
  EButtonCombo m_last_button_combo = COMBO_NONE;

  // Set this if we want to simulate the "TaruKonga" DK Bongo controller
  bool m_simulate_konga = false;

public:
  // Constructor
  CSIDevice_GCController(SIDevices device, int device_number);

  // Run the SI Buffer
  int RunBuffer(u8* buffer, int length) override;

  // Return true on new data
  bool GetData(u32& hi, u32& low) override;

  // Send a command directly
  void SendCommand(u32 command, u8 poll) override;

  // Savestate support
  void DoState(PointerWrap& p) override;

  virtual GCPadStatus GetPadStatus();
  virtual u32 MapPadStatus(const GCPadStatus& pad_status);
  virtual EButtonCombo HandleButtonCombos(const GCPadStatus& pad_status);

  // Send and Receive pad input from network
  static bool NetPlay_GetInput(int pad_num, GCPadStatus* status);
  static int NetPlay_InGamePadToLocalPad(int pad_num);

  // Direct rumble to the right GC Controller
  static void Rumble(int pad_num, ControlState strength);

protected:
  void Calibrate();
  void HandleMoviePadStatus(GCPadStatus* pad_status);
};

// "TaruKonga", the DK Bongo controller
class CSIDevice_TaruKonga : public CSIDevice_GCController
{
public:
  CSIDevice_TaruKonga(SIDevices device, int device_number)
      : CSIDevice_GCController(device, device_number)
  {
    m_simulate_konga = true;
  }
};
}  // namespace SerialInterface
