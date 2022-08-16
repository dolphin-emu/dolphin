// Copyright 2013 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/SI/SI_DeviceBaseboard.h"

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/HW/SI/SI_DeviceGCController.h"

namespace SerialInterface
{
CSIDevice_Baseboard::CSIDevice_Baseboard(SIDevices device, int device_number)
    : ISIDevice(device, device_number)
{
}

int CSIDevice_Baseboard::RunBuffer(u8* buffer, int request_length)
{
  ISIDevice::RunBuffer(buffer, request_length);

  switch (buffer[0])
  {
  case 0x00: // CMD_RESET
  {
    // Bitwise OR the SI ID against a dipswitch state (progressive in this case)
    u32 id = Common::swap32(SI_BASEBOARD | 0x100);
    std::memcpy(buffer, &id, sizeof(id));
    return sizeof(id);
    break;
  }
  case 0x70: // CMD_GCAM
  {
    CSIDevice_Baseboard::HandleSubCommand(buffer);

    buffer[1] = -2; // what and why

    int csum = 0;
    for( int i=0; i<0x7F; ++i )
      csum += buffer[i];

    buffer[0x7f] = ~csum;

    INFO_LOG_FMT(SERIALINTERFACE, "Subcommand send back: {:02x} {:02x} {:02x} {:02x} {:02x}",
                 buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]);

    // (tmbinc) hotfix: delay output by one command to work around their broken parser. this took me a month to find. ARG!
    static unsigned char last[2][0x80];
    static int lastptr[2];
    memcpy(last + 1, buffer, 0x80);
    memcpy(buffer, last, 0x80);
    memcpy(last, last + 1, 0x80);

    lastptr[1] = request_length;
    request_length = lastptr[0];
    lastptr[0] = lastptr[1];

    break;
  }
  default:
  {
    ERROR_LOG_FMT(SERIALINTERFACE, "Unhandled SI command {:02x} {:02x} {:02x} {:02x} {:02x}",
                  buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]);
    break;
  }
  }

  return 0;
}

void CSIDevice_Baseboard::HandleSubCommand(u8* buffer)
{
  switch (buffer[2])
  {
  case 0x10:
  {
    INFO_LOG_FMT(SERIALINTERFACE, "Command 10 {:02x} (READ STATUS&SWITCHES)", buffer[2]);
    buffer[0] = 0x10;
    buffer[1] = 0x2;
    buffer[2] = 0xFF;
    buffer[3] = 0xfe;
    break;
  }
  case 0x15:
  {
    INFO_LOG_FMT(SERIALINTERFACE, "Command 15, {:02x} (READ FIRM VERSION)", buffer[2]);
    buffer[1] = 0x15;
    buffer[2] = 0x02;
     // FIRM VERSION
    buffer[3] = 0x00;
    buffer[4] = 0x44;
    break;
  }
  case 0x16:
  {
    INFO_LOG_FMT(SERIALINTERFACE, "Command 16, {:02x} (READ FPGA VERSION)", buffer[2]);
    buffer[1] = 0x16;
    buffer[2] = 0x02;
     // FPGA VERSION
    buffer[3] = 0x07;
    buffer[4] = 0x09;
    break;
  }
  case 0x1f:
  {
    INFO_LOG_FMT(SERIALINTERFACE, "Command 1f, {:02x} (REGION)", buffer[2]);
    unsigned char string[] =  
      "\x00\x00\x30\x00"
      "\x01\xfe\x00\x00" // JAPAN
      //"\x02\xfd\x00\x00" // USA
      //"\x03\xfc\x00\x00" // export
      "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff";
    buffer[1] = 0x1f;
    buffer[2] = 0x14;

    for( int i=0; i<0x14; ++i )
      buffer[(3 + i)] = string[i];
    break;
  }
  default:
  {
    ERROR_LOG_FMT(SERIALINTERFACE, "Unhandled SI subcommand {:02x}", buffer[2]);
    break;
  }
  }
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
