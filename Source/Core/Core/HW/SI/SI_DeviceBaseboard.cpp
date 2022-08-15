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

  int position = 0;

  switch (buffer[position])
  {
  case 0x00: // CMD_RESET
  {
    // Bitwise OR the SI ID against a dipswitch state (progressive in this case)
    u32 id = Common::swap32(SI_BASEBOARD | 0x100);
    std::memcpy(buffer, &id, sizeof(id));
    return sizeof(id);
    //position = request_length;
  }
  case 0x70: // CMD_GCAM
  {
    // "calculate checksum over buffer"
    int csum = 0;
    for (int i=0; i < request_length; ++i)
      csum += buffer[i];

    // fix this
    unsigned char res[0x80];
    int resp = 0;

    int real_len = buffer[1];

    memset(res, 0, 0x80);
    buffer[1] = 1;
    buffer[2] = 1;

    while (2 < real_len+2)
    {
      CSIDevice_Baseboard::HandleSubCommand(buffer[2]);
    }
    // fix this
    int len = resp - 2;
    res[1] = len;
    csum = 0;
    char logptr[1024];
    char *log = logptr;

    for( int i=0; i<0x7F; ++i )
    {
      buffer[i] = res[i];
      csum += res[i];
      log += sprintf(log, "%02x ", buffer[i]);
    }

    buffer[0x7f] = ~csum;
    INFO_LOG_FMT(SERIALINTERFACE, "Command send back: {:02x}", logptr);

    // (tmbinc) hotfix: delay output by one command to work around their broken parser. this took me a month to find. ARG!
    static unsigned char last[2][0x80];
    static int lastptr[2];

    memcpy(last + 1, buffer, 0x80);
    memcpy(buffer, last, 0x80);
    memcpy(last, last + 1, 0x80);

    lastptr[1] = request_length;
    request_length = lastptr[0];
    lastptr[0] = lastptr[1];

    position = request_length;
  }
  default:
  {
    ERROR_LOG_FMT(SERIALINTERFACE, "Unhandled SI command {:02x} {:02x} {:02x} {:02x} {:02x}",
                  buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]);
    position = request_length;
  }
  }

  return position;
}

unsigned char CSIDevice_Baseboard::HandleSubCommand(u8 command)
{
  unsigned char buffer[0x80];

  switch (command)
  {
  case 0x10:
  {
    INFO_LOG_FMT(SERIALINTERFACE, "Command 10 {:02x} (READ STATUS&SWITCHES)", command);
    buffer[1] = 0x10;
    buffer[2] = 0x2;
    buffer[3] = 0xFF;
    buffer[4] = 0xfe;
    break;
  }
  case 0x15:
  {
    INFO_LOG_FMT(SERIALINTERFACE, "Command 15, {:02x} (READ FIRM VERSION)", command);
    buffer[1] = 0x15;
    buffer[2] = 0x02;
     // FIRM VERSION
    buffer[3] = 0x00;
    buffer[4] = 0x44;
    break;
  }
  case 0x16:
  {
    INFO_LOG_FMT(SERIALINTERFACE, "Command 16, {:02x} (READ FPGA VERSION)", command);
    buffer[1] = 0x16;
    buffer[2] = 0x02;
     // FPGA VERSION
    buffer[3] = 0x07;
    buffer[4] = 0x09;
    break;
  }
  case 0x1f:
  {
    INFO_LOG_FMT(SERIALINTERFACE, "Command 1f, {:02x} (REGION)", command);
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
    ERROR_LOG_FMT(SERIALINTERFACE, "Unhandled SI subcommand {:02x}", command);
  }
  }

  return reinterpret_cast<unsigned char>(buffer);
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
