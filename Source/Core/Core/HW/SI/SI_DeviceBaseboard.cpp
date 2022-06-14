// Copyright 2013 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/SI/SI_DeviceBaseboard.h"

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"
#include "Core/HW/SI/SI_Device.h"

namespace SerialInterface
{
CSIDevice_Baseboard::CSIDevice_Baseboard(SIDevices device, int device_number)
    : ISIDevice(device, device_number)
{
}

int CSIDevice_Baseboard::RunBuffer(u8* buffer, int request_length)
{
  ISIDevice::RunBuffer(buffer, request_length);

  const auto command = static_cast<EBufferCommands>(buffer[0]);

  switch (command)
  {
  case EBufferCommands::CMD_RESET:
  {
    // Bitwise OR the SI ID against a dipswitch state (progressive in this case)
    u32 id = Common::swap32(SI_BASEBOARD | 0x100);
    std::memcpy(buffer, &id, sizeof(id));
    return sizeof(id);
  }
  default:
  {
    ERROR_LOG_FMT(SERIALINTERFACE, "Unhandled SI command {:02x} {:02x} {:02x} {:02x} {:02x}",
                  buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]);
  }
  }

  return 0;
}

bool CSIDevice_Baseboard::GetData(u32& hi, u32& low)
{
  low = 0;
  hi = 0x00800000;
  return true;
}

void CSIDevice_Baseboard::SendCommand(u32 command, u8 poll)
{
  ERROR_LOG_FMT(SERIALINTERFACE, "Unhandled direct SI command ({:#x})", static_cast<u8>(command));
}

}  // namespace SerialInterface
