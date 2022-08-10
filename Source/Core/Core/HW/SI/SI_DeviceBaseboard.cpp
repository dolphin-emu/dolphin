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
  case EBufferCommands::CMD_GCAM:
  {
    // "calculate checksum over buffer"
    int csum = 0;
    for (int i=0; i < request_length; ++i)
      csum += buffer[i];

    unsigned char res[0x80];
    int resp = 0;

    int real_len = buffer[1];
    int p = 2;

    static int d10_1 = 0xfe;

    memset(res, 0, 0x80);
    res[resp++] = 1;
    res[resp++] = 1;


#define ptr(x) buffer[(2 + x)]
    while (2 < real_len+2)
    {
      switch (ptr(0))
      {
        case 0x10:
        {
          INFO_LOG_FMT(SERIALINTERFACE, "Command 10 {:02x} (READ STATUS&SWITCHES)", ptr(1));
          // GCPadStatus PadStatus;
          // memset(&PadStatus, 0 ,sizeof(PadStatus));
          // Pad::GetStatus(ISIDevice::GetDeviceNumber());
          res[resp++] = 0x10;
          res[resp++] = 0x2;
          int d10_0 = 0xFF;

          res[resp++] = d10_0;
          res[resp++] = d10_1;
          break;
        }
        case 0x11:
        {
          INFO_LOG_FMT(SERIALINTERFACE, "Command 11, {:02x} (READ SERIAL NR)", ptr(1));
          char string[] = "AADE-01A14964511";
          res[resp++] = 0x11;
          res[resp++] = 0x10;
          memcpy(res + resp, string, 0x10);
          resp += 0x10;
          break;
        }
        case 0x15:
        {
          INFO_LOG_FMT(SERIALINTERFACE, "Command 15, {:02x} (READ FIRM VERSION)", ptr(1));
          res[resp++] = 0x15;
          res[resp++] = 0x02;
           // FIRM VERSION
          res[resp++] = 0x00;
          res[resp++] = 0x44;
          break;
        }
        case 0x16:
        {
          INFO_LOG_FMT(SERIALINTERFACE, "Command 16, {:02x} (READ FPGA VERSION)", ptr(1));
          res[resp++] = 0x16;
          res[resp++] = 0x02;
           // FPGA VERSION
          res[resp++] = 0x07;
          res[resp++] = 0x09;
          break;
        }
        case 0x1f:
        {
          INFO_LOG_FMT(SERIALINTERFACE, "Command 1f, {:02x} {:02x} {:02x} {:02x} {:02x} (REGION)", ptr(1), ptr(2), ptr(3), ptr(4), ptr(5));
          unsigned char string[] =  
            "\x00\x00\x30\x00"
            "\x01\xfe\x00\x00" // JAPAN
            //"\x02\xfd\x00\x00" // USA
            //"\x03\xfc\x00\x00" // export
            "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff";
          res[resp++] = 0x1f;
          res[resp++] = 0x14;

          for( int i=0; i<0x14; ++i )
            res[resp++] = string[i];
          break;
        }
      }
      ERROR_LOG_FMT(SERIALINTERFACE, "Unhandled SI subcommand {:02x} {:02x} {:02x} {:02x} {:02x}",
                    ptr(0), ptr(1), ptr(2), ptr(3), ptr(4));
    }
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
