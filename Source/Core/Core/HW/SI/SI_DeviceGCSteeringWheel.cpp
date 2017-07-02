// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/SI/SI_DeviceGCSteeringWheel.h"

#include <cstring>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
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
  EBufferCommands command = static_cast<EBufferCommands>(buffer[3]);

  // Handle it
  switch (command)
  {
  case CMD_RESET:
  case CMD_ID:
  {
    constexpr u32 id = SI_GC_STEERING;
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

    // The GC Steering Wheel appears to have combined pedals
    // (both the Accelerate and Brake pedals are mapped to a single axis)
    // We use the stickY axis for the pedals.
    if (pad_status.stickY < 128)
      low |= (u32)((u8)(255 - ((pad_status.stickY & 0x7f) * 2)) << 16);  // All 8 bits (Brake)
    if (pad_status.stickY >= 128)
      low |= (u32)((u8)((pad_status.stickY & 0x7f) * 2) << 24);  // All 8 bits (Accelerate)

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
    // 0 = left strong, 127 = left weak, 128 = right weak, 255 = right strong
    unsigned int strength = wheel_command.parameter1;

    // 06 = motor on, 04 = motor off
    unsigned int type = wheel_command.parameter2;

    // get the correct pad number that should rumble locally when using netplay
    const int pad_num = NetPlay_InGamePadToLocalPad(m_device_number);

    if (pad_num < 4)
    {
      if (type == 0x06)
      {
        // map 0..255 to -1.0..1.0
        ControlState mapped_strength = strength / 127.5 - 1;
        Pad::Rumble(pad_num, mapped_strength);
      }
      else
      {
        Pad::Rumble(pad_num, 0);
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
