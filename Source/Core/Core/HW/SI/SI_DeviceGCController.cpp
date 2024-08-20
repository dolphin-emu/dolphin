// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/SI/SI_DeviceGCController.h"

#include <cstring>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/Swap.h"
#include "Core/Config/MainSettings.h"
#include "Core/CoreTiming.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/HW/SystemTimers.h"
#include "Core/Movie.h"
#include "Core/NetPlayProto.h"
#include "Core/System.h"
#include "InputCommon/GCPadStatus.h"

namespace SerialInterface
{
// --- standard GameCube controller ---
CSIDevice_GCController::CSIDevice_GCController(Core::System& system, SIDevices device,
                                               int device_number)
    : ISIDevice(system, device, device_number)
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

int CSIDevice_GCController::RunBuffer(u8* buffer, int request_length)
{
  // For debug logging only
  ISIDevice::RunBuffer(buffer, request_length);

  GCPadStatus pad_status = GetPadStatus();
  if (!pad_status.isConnected)
    return -1;

  // Read the command

  // Handle it
  switch (const auto command = static_cast<EBufferCommands>(buffer[0]))
  {
  case EBufferCommands::CMD_STATUS:
  case EBufferCommands::CMD_RESET:
  {
    u32 id = Common::swap32(SI_GC_CONTROLLER);
    std::memcpy(buffer, &id, sizeof(id));
    return sizeof(id);
  }

  case EBufferCommands::CMD_DIRECT:
  {
    INFO_LOG_FMT(SERIALINTERFACE, "PAD - Direct (Request length: {})", request_length);
    u32 high, low;
    GetData(high, low);
    for (int i = 0; i < 4; i++)
    {
      buffer[i + 0] = (high >> (24 - (i * 8))) & 0xff;
      buffer[i + 4] = (low >> (24 - (i * 8))) & 0xff;
    }
    return sizeof(high) + sizeof(low);
  }

  case EBufferCommands::CMD_ORIGIN:
  {
    INFO_LOG_FMT(SERIALINTERFACE, "PAD - Get Origin");

    u8* calibration = reinterpret_cast<u8*>(&m_origin);
    for (int i = 0; i < (int)sizeof(SOrigin); i++)
    {
      buffer[i] = *calibration++;
    }
    return sizeof(SOrigin);
  }

  // Recalibrate (FiRES: i am not 100 percent sure about this)
  case EBufferCommands::CMD_RECALIBRATE:
  {
    INFO_LOG_FMT(SERIALINTERFACE, "PAD - Recalibrate");

    u8* calibration = reinterpret_cast<u8*>(&m_origin);
    for (int i = 0; i < (int)sizeof(SOrigin); i++)
    {
      buffer[i] = *calibration++;
    }
    return sizeof(SOrigin);
  }

  // GameID packet, no response needed, nothing to do
  // On real hardware, this is used to configure the BlueRetro controler
  // adapter, while licensed accessories ignore this command.
  case EBufferCommands::CMD_SET_GAME_ID:
  {
    return 0;
  }

  // DEFAULT
  default:
  {
    ERROR_LOG_FMT(SERIALINTERFACE, "Unknown SI command     ({:#x})", static_cast<u8>(command));
    PanicAlertFmt("SI: Unknown command ({:#x})", static_cast<u8>(command));
  }
  break;
  }

  return 0;
}

void CSIDevice_GCController::HandleMoviePadStatus(Movie::MovieManager& movie, int device_number,
                                                  GCPadStatus* pad_status)
{
  movie.SetPolledDevice();
  if (NetPlay_GetInput(device_number, pad_status))
  {
  }
  else if (movie.IsPlayingInput())
  {
    movie.PlayController(pad_status, device_number);
    movie.InputUpdate();
  }
  else if (movie.IsRecordingInput())
  {
    movie.RecordInput(pad_status, device_number);
    movie.InputUpdate();
  }
  else
  {
    movie.CheckPadStatus(pad_status, device_number);
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

  HandleMoviePadStatus(m_system.GetMovie(), m_device_number, &pad_status);

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
      m_timer_button_combo_start = m_system.GetCoreTiming().GetTicks();
  }

  if (m_last_button_combo != COMBO_NONE)
  {
    const u64 current_time = m_system.GetCoreTiming().GetTicks();
    const u32 ticks_per_second = m_system.GetSystemTimers().GetTicksPerSecond();
    if (u32(current_time - m_timer_button_combo_start) > ticks_per_second * 3)
    {
      if (m_last_button_combo == COMBO_RESET)
      {
        INFO_LOG_FMT(SERIALINTERFACE, "PAD - COMBO_RESET");
        m_system.GetProcessorInterface().ResetButton_Tap();
      }
      else if (m_last_button_combo == COMBO_ORIGIN)
      {
        INFO_LOG_FMT(SERIALINTERFACE, "PAD - COMBO_ORIGIN");
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

  if (static_cast<EDirectCommands>(controller_command.command) == EDirectCommands::CMD_WRITE)
  {
    const u32 type = controller_command.parameter1;  // 0 = stop, 1 = rumble, 2 = stop hard

    // get the correct pad number that should rumble locally when using netplay
    const int pad_num = NetPlay_InGamePadToLocalPad(m_device_number);

    if (pad_num < 4)
    {
      const SIDevices device = m_system.GetSerialInterface().GetDeviceType(pad_num);
      if (type == 1)
        CSIDevice_GCController::Rumble(pad_num, 1.0, device);
      else
        CSIDevice_GCController::Rumble(pad_num, 0.0, device);
    }

    if (poll == 0)
    {
      m_mode = controller_command.parameter2;
      INFO_LOG_FMT(SERIALINTERFACE, "PAD {} set to mode {}", m_device_number, m_mode);
    }
  }
  else if (controller_command.command != 0x00)
  {
    // Costis sent 0x00 in some demos :)
    ERROR_LOG_FMT(SERIALINTERFACE, "Unknown direct command     ({:#x})", command);
    PanicAlertFmt("SI: Unknown direct command");
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

CSIDevice_TaruKonga::CSIDevice_TaruKonga(Core::System& system, SIDevices device, int device_number)
    : CSIDevice_GCController(system, device, device_number)
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
