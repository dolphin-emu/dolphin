// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/SI/SI_DeviceDanceMat.h"

#include <cstring>

#include "Common/CommonTypes.h"
#include "Common/Swap.h"
#include "InputCommon/GCPadStatus.h"

namespace SerialInterface
{
CSIDevice_DanceMat::CSIDevice_DanceMat(Core::System& system, SIDevices device, int device_number)
    : CSIDevice_GCController(system, device, device_number)
{
}

int CSIDevice_DanceMat::RunBuffer(u8* buffer, int request_length)
{
  // Read the command
  const auto command = static_cast<EBufferCommands>(buffer[0]);
  if (command == EBufferCommands::CMD_STATUS)
  {
    ISIDevice::RunBuffer(buffer, request_length);

    u32 id = Common::swap32(SI_DANCEMAT);
    std::memcpy(buffer, &id, sizeof(id));
    return sizeof(id);
  }
  return CSIDevice_GCController::RunBuffer(buffer, request_length);
}

u32 CSIDevice_DanceMat::MapPadStatus(const GCPadStatus& pad_status)
{
  // Map the dpad to the blue arrows, the buttons to the orange arrows
  // Z = + button, Start = - button. Active Life Mats have a different layout.
  u16 map = 0;
  if (pad_status.button & PAD_BUTTON_UP)
    map |= 0x8;
  if (pad_status.button & PAD_BUTTON_DOWN)
    map |= 0x4;
  if (pad_status.button & PAD_BUTTON_LEFT)
    map |= 0x1;
  if (pad_status.button & PAD_BUTTON_RIGHT)
    map |= 0x2;
  if (pad_status.button & PAD_BUTTON_Y)
    map |= 0x400;  // Only Active Life Mat has this button. Maps as + button.
  if (pad_status.button & PAD_BUTTON_A)
    map |= 0x100;
  if (pad_status.button & PAD_BUTTON_B)
    map |= 0x200;
  if (pad_status.button & PAD_BUTTON_X)
    map |= 0x800;  // Only Active Life Mat has this button. Maps as Right Foot Right.
  if (pad_status.button & PAD_TRIGGER_Z)
    map |= 0x10;
  if (pad_status.button & PAD_BUTTON_START)
    map |= 0x1000;

  return (u32)(map << 16) | 0x8080;
}

bool CSIDevice_DanceMat::GetData(u32& hi, u32& low)
{
  CSIDevice_GCController::GetData(hi, low);

  // Identifies the dance mat
  low = 0x8080ffff;

  return true;
}
}  // namespace SerialInterface
