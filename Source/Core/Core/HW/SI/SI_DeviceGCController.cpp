// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/SI/SI_DeviceGCController.h"

#include <cstring>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/Swap.h"
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
  // Here we set origin to perfectly centered values.
  // This purposely differs from real hardware which sets origin to current input state.
  // That behavior is less than ideal as the user may have inadvertently moved from neutral.
  // The X+Y+Start button combo can override this if desired.
  m_origin.origin_stick_x = GCPadStatus::MAIN_STICK_CENTER_X;
  m_origin.origin_stick_y = GCPadStatus::MAIN_STICK_CENTER_Y;
  m_origin.substick_x = GCPadStatus::C_STICK_CENTER_X;
  m_origin.substick_y = GCPadStatus::C_STICK_CENTER_Y;
}

int CSIDevice_GCController::RunBuffer(u8* buffer, int length)
{
  // For debug logging only
  ISIDevice::RunBuffer(buffer, length);

  GCPadStatus pad_status = GetPadStatus();
  if (!pad_status.isConnected)
  {
    u32 reply = Common::swap32(SI_ERROR_NO_RESPONSE);
    std::memcpy(buffer, &reply, sizeof(reply));
    return 4;
  }

  // Read the command
  EBufferCommands command = static_cast<EBufferCommands>(buffer[0]);

  // Handle it
  switch (command)
  {
  case CMD_RESET:
  case CMD_ID:
  {
    u32 id = Common::swap32(SI_GC_CONTROLLER);
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
      buffer[i + 0] = (high >> (24 - (i * 8))) & 0xff;
      buffer[i + 4] = (low >> (24 - (i * 8))) & 0xff;
    }
  }
  break;

  case CMD_ORIGIN:
  {
    INFO_LOG(SERIALINTERFACE, "PAD - Get Origin");

    u8* calibration = reinterpret_cast<u8*>(&m_origin);
    for (int i = 0; i < (int)sizeof(SOrigin); i++)
    {
      buffer[i] = *calibration++;
    }
  }
  break;

  // Recalibrate (FiRES: i am not 100 percent sure about this)
  case CMD_RECALIBRATE:
  {
    INFO_LOG(SERIALINTERFACE, "PAD - Recalibrate");

    u8* calibration = reinterpret_cast<u8*>(&m_origin);
    for (int i = 0; i < (int)sizeof(SOrigin); i++)
    {
      buffer[i] = *calibration++;
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

  // Our GCAdapter code sets PAD_GET_ORIGIN when a new device has been connected.
  // Watch for this to calibrate real controllers on connection.
  if (pad_status.button & PAD_GET_ORIGIN)
    SetOrigin(pad_status);

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

  if (!pad_status.isConnected)
  {
    hi = 0x80000000;
    return true;
  }

  if (HandleButtonCombos(pad_status) == COMBO_ORIGIN)
    pad_status.button |= PAD_GET_ORIGIN;

  hi = MapPadStatus(pad_status);

  // Low bits are packed differently per mode
  if (m_mode == 0 || m_mode == 5 || m_mode == 6 || m_mode == 7)
  {
    low = (pad_status.analogB >> 4);               // Top 4 bits
    low |= ((pad_status.analogA >> 4) << 4);       // Top 4 bits
    low |= ((pad_status.triggerRight >> 4) << 8);  // Top 4 bits
    low |= ((pad_status.triggerLeft >> 4) << 12);  // Top 4 bits
    low |= ((pad_status.substickY) << 16);         // All 8 bits
    low |= ((pad_status.substickX) << 24);         // All 8 bits
  }
  else if (m_mode == 1)
  {
    low = (pad_status.analogB >> 4);             // Top 4 bits
    low |= ((pad_status.analogA >> 4) << 4);     // Top 4 bits
    low |= (pad_status.triggerRight << 8);       // All 8 bits
    low |= (pad_status.triggerLeft << 16);       // All 8 bits
    low |= ((pad_status.substickY >> 4) << 24);  // Top 4 bits
    low |= ((pad_status.substickX >> 4) << 28);  // Top 4 bits
  }
  else if (m_mode == 2)
  {
    low = pad_status.analogB;                       // All 8 bits
    low |= pad_status.analogA << 8;                 // All 8 bits
    low |= ((pad_status.triggerRight >> 4) << 16);  // Top 4 bits
    low |= ((pad_status.triggerLeft >> 4) << 20);   // Top 4 bits
    low |= ((pad_status.substickY >> 4) << 24);     // Top 4 bits
    low |= ((pad_status.substickX >> 4) << 28);     // Top 4 bits
  }
  else if (m_mode == 3)
  {
    // Analog A/B are always 0
    low = pad_status.triggerRight;         // All 8 bits
    low |= (pad_status.triggerLeft << 8);  // All 8 bits
    low |= (pad_status.substickY << 16);   // All 8 bits
    low |= (pad_status.substickX << 24);   // All 8 bits
  }
  else if (m_mode == 4)
  {
    low = pad_status.analogB;        // All 8 bits
    low |= pad_status.analogA << 8;  // All 8 bits
    // triggerLeft/Right are always 0
    low |= pad_status.substickY << 16;  // All 8 bits
    low |= pad_status.substickX << 24;  // All 8 bits
  }

  return true;
}

u32 CSIDevice_GCController::MapPadStatus(const GCPadStatus& pad_status)
{
  // Thankfully changing mode does not change the high bits ;)
  u32 hi = 0;
  hi = pad_status.stickY;
  hi |= pad_status.stickX << 8;
  hi |= (pad_status.button | PAD_USE_ORIGIN) << 16;
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
    const u64 current_time = CoreTiming::GetTicks();
    if (u32(current_time - m_timer_button_combo_start) > SystemTimers::GetTicksPerSecond() * 3)
    {
      if (m_last_button_combo == COMBO_RESET)
      {
        INFO_LOG(SERIALINTERFACE, "PAD - COMBO_RESET");
        ProcessorInterface::ResetButton_Tap();
      }
      else if (m_last_button_combo == COMBO_ORIGIN)
      {
        INFO_LOG(SERIALINTERFACE, "PAD - COMBO_ORIGIN");
        SetOrigin(pad_status);
      }

      m_last_button_combo = COMBO_NONE;
      return temp_combo;
    }
  }

  return COMBO_NONE;
}

void CSIDevice_GCController::SetOrigin(const GCPadStatus& pad_status)
{
  m_origin.origin_stick_x = pad_status.stickX;
  m_origin.origin_stick_y = pad_status.stickY;
  m_origin.substick_x = pad_status.substickX;
  m_origin.substick_y = pad_status.substickY;
  m_origin.trigger_left = pad_status.triggerLeft;
  m_origin.trigger_right = pad_status.triggerRight;
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

    // get the correct pad number that should rumble locally when using netplay
    const int pad_num = NetPlay_InGamePadToLocalPad(m_device_number);

    if (pad_num < 4)
    {
      if (type == 1)
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
  p.Do(m_origin);
  p.Do(m_mode);
  p.Do(m_timer_button_combo_start);
  p.Do(m_last_button_combo);
}

CSIDevice_TaruKonga::CSIDevice_TaruKonga(SIDevices device, int device_number)
    : CSIDevice_GCController(device, device_number)
{
}

bool CSIDevice_TaruKonga::GetData(u32& hi, u32& low)
{
  CSIDevice_GCController::GetData(hi, low);

  // Unsets the first 16 bits (StickX/Y), PAD_USE_ORIGIN,
  // and all buttons except: A, B, X, Y, Start, R
  hi &= HI_BUTTON_MASK;

  return true;
}

}  // namespace SerialInterface
