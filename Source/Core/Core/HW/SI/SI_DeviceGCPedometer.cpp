// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/SI/SI_DeviceGCPedometer.h"

#include <cstring>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/Swap.h"
#include "Core/HW/SI/SI_Device.h"

namespace SerialInterface
{
// Gamecube Pedometer aka Inro-Kun
CSIDevice_GCPedometer::CSIDevice_GCPedometer(SIDevices device, int device_number)
    : ISIDevice(device, device_number)
{
}

int CSIDevice_GCPedometer::RunBuffer(u8* buffer, int length)
{
  // For debug logging only
  ISIDevice::RunBuffer(buffer, length);

  // Read the command
  EBufferCommands command = static_cast<EBufferCommands>(buffer[0]);

  // Process command
  switch (command)
  {
  case CMD_RESET:
  {
    // Return joybus ID for buffer commands
    u32 id = Common::swap32(SI_GC_PEDOMETER);
    std::memcpy(buffer, &id, sizeof(id));
    break;
  }

  // Return data from pedometer
  case CMD_DATA:
  {
    // Pedometer expects 0x51 bytes from buffer
    if (length == 0x51)
    {
      // Clear existing data on SI buffer for pedometer response
      std::fill_n(buffer, 0, length);

      // Copy 1st 3 characters of the name field - Buffer positions 0x00, 0x01, 0x02
      buffer[0x00] = m_data.name_high[0];
      buffer[0x01] = m_data.name_high[1];
      buffer[0x02] = m_data.name_high[2];

      // Copy 2nd 3 characters of the name field - Buffer positions 0x05, 0x06, 0x07
      buffer[0x05] = m_data.name_low[0];
      buffer[0x06] = m_data.name_low[1];
      buffer[0x07] = m_data.name_low[2];

      // Copy length of step and gender - Buffer positions 0x08, 0x09
      buffer[0x08] = m_data.gender;
      buffer[0x09] = m_data.step_length;

      // Copy height and weight - Buffer positions 0x0A, 0x0B
      buffer[0x0A] = m_data.height;
      buffer[0x0B] = m_data.weight;

      // Copy total steps taken - Buffer positions 0x0D, 0x0E, 0x0F
      buffer[0x0D] = m_data.total_steps[0];
      buffer[0x0E] = m_data.total_steps[1];
      buffer[0x0F] = m_data.total_steps[2];

      // Copy total meters walked - Buffer positions 0x12, 0x13, 0x0C
      buffer[0x12] = m_data.total_meters[0];
      buffer[0x13] = m_data.total_meters[1];
      buffer[0x0C] = m_data.total_meters[2];
    }
    break;
  }

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

// SendCommand
void CSIDevice_GCPedometer::SendCommand(u32 command, u8 poll)
{
  UCommand controller_command(command);

  switch (controller_command.command)
  {
  case CMD_RESET:
    // Prepare pedometer to return joybus ID after direct command
    m_reset = true;
    break;

  case CMD_WRITE:
  {
    // Acknowledge any previous commands sent by buffer
    m_acknowledge = true;
    INFO_LOG(SERIALINTERFACE, "Pedometer Write 0x%x", command);
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

// GetData
bool CSIDevice_GCPedometer::GetData(u32& hi, u32& low)
{
  // Return joybus ID for direct commands
  if (m_reset)
  {
    hi = Common::swap32(SI_GC_PEDOMETER);
    low = 0x0;
    m_reset = false;
  }

  // Return acknowledgement - Bit 31 of high byte set
  else if (m_acknowledge)
  {
    hi = DATA_ACKNOWLEDGE_HI;
    low = 0x0;
    m_acknowledge = false;
  }

  return true;
}

// Savestate support
void CSIDevice_GCPedometer::DoState(PointerWrap& p)
{
  p.Do(m_data);
  p.Do(m_acknowledge);
  p.Do(m_reset);
}
}  // namespace SerialInterface
