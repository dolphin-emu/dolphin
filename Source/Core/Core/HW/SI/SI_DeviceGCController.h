// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include "Core/HW/GCPad.h"
#include "Core/HW/SI/SI_Device.h"
#include "InputCommon/GCPadStatus.h"

namespace Movie
{
class MovieManager;
}

namespace SerialInterface
{
class CSIDevice_GCController : public ISIDevice
{
protected:
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
  };

  enum EButtonCombo
  {
    COMBO_NONE = 0,
    COMBO_ORIGIN,
    COMBO_RESET
  };

  // struct to compare input against
  // Set on connection to perfect neutral values
  // (standard pad only) Set on button combo to current input state
  SOrigin m_origin = {};

  // PADAnalogMode
  // Dunno if we need to do this, game/lib should set it?
  u8 m_mode = 0x3;

  // Timer to track special button combos:
  // y, X, start for 3 seconds updates origin with current status
  //   Technically, the above is only on standard pad, wavebird does not support it for example
  // b, x, start for 3 seconds triggers reset (PI reset button interrupt)
  u64 m_timer_button_combo_start = 0;
  // Type of button combo from the last/current poll
  EButtonCombo m_last_button_combo = COMBO_NONE;

public:
  // Constructor
  CSIDevice_GCController(Core::System& system, SIDevices device, int device_number);

  // Run the SI Buffer
  int RunBuffer(u8* buffer, int request_length) override;

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
  static void Rumble(int pad_num, ControlState strength, SIDevices device);

  static void HandleMoviePadStatus(Movie::MovieManager& movie, int device_number,
                                   GCPadStatus* pad_status);

protected:
  void SetOrigin(const GCPadStatus& pad_status);
};

// "TaruKonga", the DK Bongo controller
class CSIDevice_TaruKonga final : public CSIDevice_GCController
{
public:
  CSIDevice_TaruKonga(Core::System& system, SIDevices device, int device_number);

  bool GetData(u32& hi, u32& low) override;

  static constexpr u32 HI_BUTTON_MASK =
      (PAD_BUTTON_A | PAD_BUTTON_B | PAD_BUTTON_X | PAD_BUTTON_Y | PAD_BUTTON_START | PAD_TRIGGER_R)
      << 16;
};
}  // namespace SerialInterface
