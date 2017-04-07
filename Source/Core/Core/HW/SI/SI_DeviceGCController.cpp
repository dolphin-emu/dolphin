// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/SI/SI_DeviceGCController.h"

#include <cstring>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Core/CoreTiming.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/HW/SystemTimers.h"
#include "Core/Movie.h"
#include "Core/NetPlayProto.h"
#include "InputCommon/GCPadStatus.h"

namespace SerialInterface
{
// --- standard GameCube controller ---
CSIDevice_GCController::CSIDevice_GCController(SIDevices device, int device_number)
    : ISIDevice(device, device_number)
{
}

void CSIDevice_GCController::Calibrate()
{
  GCPadStatus pad_origin = GetPadStatus();
  memset(&m_origin, 0, sizeof(SOrigin));
  m_origin.button = pad_origin.button;
  m_origin.origin_stick_x = pad_origin.stickX;
  m_origin.origin_stick_y = pad_origin.stickY;
  m_origin.substick_x = pad_origin.substickX;
  m_origin.substick_y = pad_origin.substickY;
  m_origin.trigger_left = pad_origin.triggerLeft;
  m_origin.trigger_right = pad_origin.triggerRight;

  m_calibrated = true;
}

int CSIDevice_GCController::RunBuffer(u8* buffer, int length)
{
  // For debug logging only
  ISIDevice::RunBuffer(buffer, length);

  // Read the command
  EBufferCommands command = static_cast<EBufferCommands>(buffer[3]);

  // Handle it
  switch (command)
  {
  case CMD_RESET:
  case CMD_ID:
  {
    constexpr u32 id = SI_GC_CONTROLLER;
    std::memcpy(buffer, &id, sizeof(id));
    break;
  }

  case CMD_DIRECT:
  {
    INFO_LOG(SERIALINTERFACE, "PAD - Direct (Length: %d)", length);
    u32 high, low;
    GetData(high, low);
    for (int i = 0; i < (length - 1) / 2; i++)
    {
      buffer[i + 0] = (high >> (i * 8)) & 0xff;
      buffer[i + 4] = (low >> (i * 8)) & 0xff;
    }
  }
  break;

  case CMD_ORIGIN:
  {
    INFO_LOG(SERIALINTERFACE, "PAD - Get Origin");

    if (!m_calibrated)
      Calibrate();

    u8* calibration = reinterpret_cast<u8*>(&m_origin);
    for (int i = 0; i < (int)sizeof(SOrigin); i++)
    {
      buffer[i ^ 3] = *calibration++;
    }
  }
  break;

  // Recalibrate (FiRES: i am not 100 percent sure about this)
  case CMD_RECALIBRATE:
  {
    INFO_LOG(SERIALINTERFACE, "PAD - Recalibrate");

    if (!m_calibrated)
      Calibrate();

    u8* calibration = reinterpret_cast<u8*>(&m_origin);
    for (int i = 0; i < (int)sizeof(SOrigin); i++)
    {
      buffer[i ^ 3] = *calibration++;
    }
  }
  break;

  // DEFAULT
  default:
  {
    ERROR_LOG(SERIALINTERFACE, "Unknown SI command     (0x%x)", command);
    PanicAlert("SI: Unknown command (0x%x)", command);
  }
  break;
  }

  return length;
}

void CSIDevice_GCController::HandleMoviePadStatus(GCPadStatus* pad_status)
{
  Movie::CallGCInputManip(pad_status, m_device_number);

  Movie::SetPolledDevice();
  if (NetPlay_GetInput(m_device_number, pad_status))
  {
  }
  else if (Movie::IsPlayingInput())
  {
    Movie::PlayController(pad_status, m_device_number);
    Movie::InputUpdate();
  }
  else if (Movie::IsRecordingInput())
  {
    Movie::RecordInput(pad_status, m_device_number);
    Movie::InputUpdate();
  }
  else
  {
    Movie::CheckPadStatus(pad_status, m_device_number);
  }
}

GCPadStatus CSIDevice_GCController::GetPadStatus()
{
  GCPadStatus pad_status = {};

  // For netplay, the local controllers are polled in GetNetPads(), and
  // the remote controllers receive their status there as well
  if (!NetPlay::IsNetPlayRunning())
  {
    pad_status = Pad::GetStatus(m_device_number);
  }

  HandleMoviePadStatus(&pad_status);
  return pad_status;
}

// GetData

// Return true on new data (max 7 Bytes and 6 bits ;)
// [00?SYXBA] [1LRZUDRL] [x] [y] [cx] [cy] [l] [r]
//  |\_ ERR_LATCH (error latched - check SISR)
//  |_ ERR_STATUS (error on last GetData or SendCmd?)
bool CSIDevice_GCController::GetData(u32& hi, u32& low)
{
  GCPadStatus pad_status = GetPadStatus();
  if (HandleButtonCombos(pad_status) == COMBO_ORIGIN)
    pad_status.button |= PAD_GET_ORIGIN;

  hi = MapPadStatus(pad_status);

  // Low bits are packed differently per mode
  if (m_mode == 0 || m_mode == 5 || m_mode == 6 || m_mode == 7)
  {
    low = (u8)(pad_status.analogB >> 4);                    // Top 4 bits
    low |= (u32)((u8)(pad_status.analogA >> 4) << 4);       // Top 4 bits
    low |= (u32)((u8)(pad_status.triggerRight >> 4) << 8);  // Top 4 bits
    low |= (u32)((u8)(pad_status.triggerLeft >> 4) << 12);  // Top 4 bits
    low |= (u32)((u8)(pad_status.substickY) << 16);         // All 8 bits
    low |= (u32)((u8)(pad_status.substickX) << 24);         // All 8 bits
  }
  else if (m_mode == 1)
  {
    low = (u8)(pad_status.analogB >> 4);               // Top 4 bits
    low |= (u32)((u8)(pad_status.analogA >> 4) << 4);  // Top 4 bits
    low |= (u32)((u8)pad_status.triggerRight << 8);    // All 8 bits
    low |= (u32)((u8)pad_status.triggerLeft << 16);    // All 8 bits
    low |= (u32)((u8)pad_status.substickY << 24);      // Top 4 bits
    low |= (u32)((u8)pad_status.substickX << 28);      // Top 4 bits
  }
  else if (m_mode == 2)
  {
    low = (u8)(pad_status.analogB);                          // All 8 bits
    low |= (u32)((u8)(pad_status.analogA) << 8);             // All 8 bits
    low |= (u32)((u8)(pad_status.triggerRight >> 4) << 16);  // Top 4 bits
    low |= (u32)((u8)(pad_status.triggerLeft >> 4) << 20);   // Top 4 bits
    low |= (u32)((u8)pad_status.substickY << 24);            // Top 4 bits
    low |= (u32)((u8)pad_status.substickX << 28);            // Top 4 bits
  }
  else if (m_mode == 3)
  {
    // Analog A/B are always 0
    low = (u8)pad_status.triggerRight;              // All 8 bits
    low |= (u32)((u8)pad_status.triggerLeft << 8);  // All 8 bits
    low |= (u32)((u8)pad_status.substickY << 16);   // All 8 bits
    low |= (u32)((u8)pad_status.substickX << 24);   // All 8 bits
  }
  else if (m_mode == 4)
  {
    low = (u8)(pad_status.analogB);               // All 8 bits
    low |= (u32)((u8)(pad_status.analogA) << 8);  // All 8 bits
    // triggerLeft/Right are always 0
    low |= (u32)((u8)pad_status.substickY << 16);  // All 8 bits
    low |= (u32)((u8)pad_status.substickX << 24);  // All 8 bits
  }

  // Unset all bits except those that represent
  // A, B, X, Y, Start and the error bits, as they
  // are not used.
  if (m_simulate_konga)
    hi &= ~0x20FFFFFF;

  return true;
}

u32 CSIDevice_GCController::MapPadStatus(const GCPadStatus& pad_status)
{
  // Thankfully changing mode does not change the high bits ;)
  u32 hi = 0;
  hi = (u32)((u8)pad_status.stickY);
  hi |= (u32)((u8)pad_status.stickX << 8);
  hi |= (u32)((u16)(pad_status.button | PAD_USE_ORIGIN) << 16);
  return hi;
}

CSIDevice_GCController::EButtonCombo
CSIDevice_GCController::HandleButtonCombos(const GCPadStatus& pad_status)
{
  // Keep track of the special button combos (embedded in controller hardware... :( )
  EButtonCombo temp_combo;
  if ((pad_status.button & 0xff00) == (PAD_BUTTON_Y | PAD_BUTTON_X | PAD_BUTTON_START))
    temp_combo = COMBO_ORIGIN;
  else if ((pad_status.button & 0xff00) == (PAD_BUTTON_B | PAD_BUTTON_X | PAD_BUTTON_START))
    temp_combo = COMBO_RESET;
  else
    temp_combo = COMBO_NONE;

  if (temp_combo != m_last_button_combo)
  {
    m_last_button_combo = temp_combo;
    if (m_last_button_combo != COMBO_NONE)
      m_timer_button_combo_start = CoreTiming::GetTicks();
  }

  if (m_last_button_combo != COMBO_NONE)
  {
    m_timer_button_combo = CoreTiming::GetTicks();
    if ((m_timer_button_combo - m_timer_button_combo_start) > SystemTimers::GetTicksPerSecond() * 3)
    {
      if (m_last_button_combo == COMBO_RESET)
      {
        ProcessorInterface::ResetButton_Tap();
      }
      else if (m_last_button_combo == COMBO_ORIGIN)
      {
        m_origin.origin_stick_x = pad_status.stickX;
        m_origin.origin_stick_y = pad_status.stickY;
        m_origin.substick_x = pad_status.substickX;
        m_origin.substick_y = pad_status.substickY;
        m_origin.trigger_left = pad_status.triggerLeft;
        m_origin.trigger_right = pad_status.triggerRight;
      }

      m_last_button_combo = COMBO_NONE;
      return temp_combo;
    }
  }

  return COMBO_NONE;
}

// SendCommand
void CSIDevice_GCController::SendCommand(u32 command, u8 poll)
{
  UCommand controller_command(command);

  switch (controller_command.command)
  {
  // Costis sent it in some demos :)
  case 0x00:
    break;

  case CMD_WRITE:
  {
    unsigned int type = controller_command.parameter1;  // 0 = stop, 1 = rumble, 2 = stop hard
    unsigned int strength = controller_command.parameter2;

    // get the correct pad number that should rumble locally when using netplay
    const int pad_num = NetPlay_InGamePadToLocalPad(m_device_number);

    if (pad_num < 4)
    {
      if (type == 1 && strength > 2)
        CSIDevice_GCController::Rumble(pad_num, 1.0);
      else
        CSIDevice_GCController::Rumble(pad_num, 0.0);
    }

    if (!poll)
    {
      m_mode = controller_command.parameter2;
      INFO_LOG(SERIALINTERFACE, "PAD %i set to mode %i", m_device_number, m_mode);
    }
  }
  break;

  default:
  {
    ERROR_LOG(SERIALINTERFACE, "Unknown direct command     (0x%x)", command);
    PanicAlert("SI: Unknown direct command");
  }
  break;
  }
}

// Savestate support
void CSIDevice_GCController::DoState(PointerWrap& p)
{
  p.Do(m_calibrated);
  p.Do(m_origin);
  p.Do(m_mode);
  p.Do(m_timer_button_combo_start);
  p.Do(m_timer_button_combo);
  p.Do(m_last_button_combo);
}
}  // namespace SerialInterface
