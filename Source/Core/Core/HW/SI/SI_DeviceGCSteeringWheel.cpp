// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/SI/SI_DeviceGCSteeringWheel.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"
#include "Core/HW/GCPad.h"

namespace SerialInterface
{
CSIDevice_GCSteeringWheel::CSIDevice_GCSteeringWheel(SIDevices device, int device_number)
    : CSIDevice_GCController(device, device_number)
{
}

int CSIDevice_GCSteeringWheel::RunBuffer(u8* buffer, int length)
{
  // For debug logging only
  ISIDevice::RunBuffer(buffer, length);

  // Read the command
  EBufferCommands command = static_cast<EBufferCommands>(buffer[0]);

  // Handle it
  switch (command)
  {
  case CMD_RESET:
  case CMD_ID:
  {
    u32 id = Common::swap32(SI_GC_STEERING);
    std::memcpy(buffer, &id, sizeof(id));
    break;
  }

  default:
    return CSIDevice_GCController::RunBuffer(buffer, length);
  }

  return length;
}

bool CSIDevice_GCSteeringWheel::GetData(u32& hi, u32& low)
{
  if (m_mode == 6)
  {
    GCPadStatus pad_status = GetPadStatus();

    hi = (u32)((u8)pad_status.stickX);  // Steering
    hi |= 0x800;                        // Pedal connected flag
    hi |= (u32)((u16)(pad_status.button | PAD_USE_ORIGIN) << 16);

    low = (u8)pad_status.triggerRight;              // All 8 bits
    low |= (u32)((u8)pad_status.triggerLeft << 8);  // All 8 bits

    // The GC Steering Wheel has 8 bit values for both the accelerator/brake.
    // Our mapping UI and GCPadEmu class weren't really designed to provide
    // input data other than what the regular GC controller has.
    //
    // Without a big redesign we really don't have a choice but to
    // use the analog stick values for accelerator/brake.
    //
    // Main-stick: up:accelerator / down:brake
    //
    // But that doesn't allow the user to press both at the same time.
    // so also provide the opposite functionality on the c-stick.
    //
    // C-stick: up:brake / down:accelerator

    // Use either the upper-half of main-stick or lower-half of c-stick for accelerator.
    const int accel_value = std::max(pad_status.stickY - GCPadStatus::MAIN_STICK_CENTER_Y,
                                     GCPadStatus::C_STICK_CENTER_Y - pad_status.substickY);

    // Use either the upper-half of c-stick or lower-half of main-stick for brake.
    const int brake_value = std::max(pad_status.substickY - GCPadStatus::C_STICK_CENTER_Y,
                                     GCPadStatus::MAIN_STICK_CENTER_Y - pad_status.stickY);

    // We must double these values because we are mapping half of a stick range to a 0..255 value.
    // We're only getting half the precison we could potentially have,
    // but we'll have to redesign our gamecube controller input to fix that.

    // All 8 bits (Accelerate)
    low |= u32(std::clamp(accel_value * 2, 0, 0xff)) << 24;

    // All 8 bits (Brake)
    low |= u32(std::clamp(brake_value * 2, 0, 0xff)) << 16;

    HandleButtonCombos(pad_status);
  }
  else
  {
    return CSIDevice_GCController::GetData(hi, low);
  }

  return true;
}

void CSIDevice_GCSteeringWheel::SendCommand(u32 command, u8 poll)
{
  UCommand wheel_command(command);

  if (wheel_command.command == CMD_FORCE)
  {
    // get the correct pad number that should rumble locally when using netplay
    const int pad_num = NetPlay_InGamePadToLocalPad(m_device_number);

    if (pad_num < 4)
    {
      // Lowest bit is the high bit of the strength field.
      const auto type = ForceCommandType(wheel_command.parameter2 >> 1);

      // Strength is a 9 bit value from 0 to 256.
      // 0 = left strong, 256 = right strong
      const u32 strength = ((wheel_command.parameter2 & 1) << 8) | wheel_command.parameter1;

      switch (type)
      {
      case ForceCommandType::MotorOn:
      {
        // Map 0..256 to -1.0..1.0
        const ControlState mapped_strength = strength / 128.0 - 1;
        Pad::Rumble(pad_num, mapped_strength);
        break;
      }
      case ForceCommandType::MotorOff:
        Pad::Rumble(pad_num, 0);
        break;
      default:
        WARN_LOG(SERIALINTERFACE, "Unknown CMD_FORCE type %i", int(type));
        break;
      }
    }

    if (!poll)
    {
      m_mode = wheel_command.parameter2;
      INFO_LOG(SERIALINTERFACE, "PAD %i set to mode %i", m_device_number, m_mode);
    }
  }
  else
  {
    return CSIDevice_GCController::SendCommand(command, poll);
  }
}
}  // namespace SerialInterface
