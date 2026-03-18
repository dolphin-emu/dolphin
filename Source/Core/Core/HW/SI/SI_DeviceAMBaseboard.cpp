// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/SI/SI_DeviceAMBaseboard.h"

#include <algorithm>
#include <numeric>
#include <string>

#include <fmt/format.h>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/Swap.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/DVD/AMMediaboard.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/MagCard/C1231BR.h"
#include "Core/HW/MagCard/C1231LR.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/HW/SystemTimers.h"
#include "Core/HW/Triforce/DeckReader.h"
#include "Core/HW/Triforce/ICCardReader.h"
#include "Core/HW/Triforce/Touchscreen.h"
#include "Core/Movie.h"
#include "Core/System.h"

#include "InputCommon/GCPadStatus.h"

namespace SerialInterface
{
using namespace AMMediaboard;

void JVSIOMessage::Start(int node)
{
  m_last_start = m_pointer;
  const u8 header[3] = {0xE0, (u8)node, 0};
  m_checksum = 0;
  AddData(header, 3, 1);
}

void JVSIOMessage::AddData(const u8* dst, std::size_t len, int sync = 0)
{
  if (m_pointer + len >= sizeof(m_message))
  {
    PanicAlertFmt("JVSIOMessage overrun!");
    return;
  }

  while (len--)
  {
    const u8 c = *dst++;
    if (!sync && ((c == 0xE0) || (c == 0xD0)))
    {
      if (m_pointer + 2 > sizeof(m_message))
      {
        PanicAlertFmt("JVSIOMessage overrun!");
        break;
      }
      m_message[m_pointer++] = 0xD0;
      m_message[m_pointer++] = c - 1;
    }
    else
    {
      if (m_pointer >= sizeof(m_message))
      {
        PanicAlertFmt("JVSIOMessage overrun!");
        break;
      }
      m_message[m_pointer++] = c;
    }

    if (!sync)
      m_checksum += c;
    sync = 0;
  }
}

void JVSIOMessage::AddData(const void* data, std::size_t len)
{
  AddData(static_cast<const u8*>(data), len);
}

void JVSIOMessage::AddData(const char* data)
{
  AddData(data, strlen(data));
}

void JVSIOMessage::AddData(u32 n)
{
  const u8 cs = n;
  AddData(&cs, 1);
}

void JVSIOMessage::End()
{
  const u32 len = m_pointer - m_last_start;
  if (m_last_start + 2 < sizeof(m_message) && len >= 3)
  {
    m_message[m_last_start + 2] = len - 2;  // assuming len <0xD0
    AddData(m_checksum + len - 2);
  }
  else
  {
    PanicAlertFmt("JVSIOMessage: Not enough space for checksum!");
  }
}

const constexpr u8 s_region_flags[] = "\x00\x00\x30\x00"
                                      //   "\x01\xfe\x00\x00"  // JAPAN
                                      "\x02\xfd\x00\x00"  // USA
                                      //"\x03\xfc\x00\x00"  // export
                                      "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff";
// AM-Baseboard device on SI
CSIDevice_AMBaseboard::CSIDevice_AMBaseboard(Core::System& system, SIDevices device,
                                             int device_number)
    : ISIDevice(system, device, device_number)
{
  // Magnetic Card Reader
  m_mag_card_settings.card_path = File::GetUserPath(D_TRIUSER_IDX);
  m_mag_card_settings.card_name = fmt::format("tricard_{}.bin", SConfig::GetInstance().GetGameID());

  switch (AMMediaboard::GetGameType())
  {
  case FZeroAX:
    m_serial_device_b = std::make_unique<MagCard::C1231BR>(&m_mag_card_settings);
    break;

  case MarioKartGP:
  case MarioKartGP2:
    m_io_ports.AddIOAdapter(std::make_unique<Triforce::MarioKartGPCommon_IOAdapter>());
    m_serial_device_b = std::make_unique<MagCard::C1231LR>(&m_mag_card_settings);
    break;

  case VirtuaStriker4:
  {
    auto slot_a = std::make_unique<Triforce::ICCardReader>(0);
    auto slot_b = std::make_unique<Triforce::ICCardReader>(1);
    m_io_ports.AddIOAdapter(
        std::make_unique<Triforce::VirtuaStriker4Common_IOAdapter>(slot_a.get(), slot_b.get()));
    m_serial_device_a = std::move(slot_a);
    m_serial_device_b = std::move(slot_b);
    break;
  }
  case VirtuaStriker4_2006:
  {
    auto slot_a = std::make_unique<Triforce::ICCardReader>(0);
    auto slot_b = std::make_unique<Triforce::ICCardReader>(1);
    m_io_ports.AddIOAdapter(
        std::make_unique<Triforce::VirtuaStriker4Common_IOAdapter>(slot_a.get(), slot_b.get()));
    m_io_ports.AddIOAdapter(
        std::make_unique<Triforce::VirtuaStriker4_2006_IOAdapter>(slot_a.get(), slot_b.get()));
    m_serial_device_a = std::move(slot_a);
    m_serial_device_b = std::move(slot_b);

    break;
  }
  case GekitouProYakyuu:
  {
    auto slot_a = std::make_unique<Triforce::ICCardReader>(0);
    auto slot_b = std::make_unique<Triforce::ICCardReader>(1);
    m_io_ports.AddIOAdapter(
        std::make_unique<Triforce::GekitouProYakyuu_IOAdapter>(slot_a.get(), slot_b.get()));
    m_serial_device_a = std::move(slot_a);
    m_serial_device_b = std::move(slot_b);

    break;
  }
  case KeyOfAvalon:
  {
    auto deck_reader = std::make_unique<Triforce::DeckReader>();
    m_io_ports.AddIOAdapter(
        std::make_unique<Triforce::KeyOfAvalon_IOAdapter>(deck_reader->GetICCardReader()));
    m_serial_device_a = std::move(deck_reader);
    m_serial_device_b = std::make_unique<Triforce::Touchscreen>();
    break;
  }

  default:
    break;
  }
}

int CSIDevice_AMBaseboard::RunBuffer(u8* buffer, int request_length)
{
  // Debug logging
  ISIDevice::RunBuffer(buffer, request_length);

  const auto& serial_interface = m_system.GetSerialInterface();

  const auto bb_command = EBufferCommands(buffer[0]);
  switch (bb_command)
  {
  case EBufferCommands::CMD_STATUS:  // Returns ID and dip switches
  {
    return CreateStatusResponse(SI_AM_BASEBOARD | 0x100, buffer);
  }
  break;
  case EBufferCommands::CMD_AM_BASEBOARD:
  {
    // FYI: Supplied `buffer` always points to 128 bytes.
    // TODO: Change the `RunBuffer` interface to make that more obvious.
    const int command_length = buffer[1];

    constexpr int command_header_size = 2;
    if (command_header_size + command_length > request_length)
    {
      ERROR_LOG_FMT(SERIALINTERFACE_AMBB, "Bad command_length: {} for request_length: {}",
                    command_length, request_length);
      return -1;
    }

    auto& data_out = m_response_buffers[m_current_response_buffer_index];

    // Need room for the checksum.
    const auto usable_out_size = data_out.size() - 1;

    constexpr int response_header_size = 2;
    u32 data_offset = response_header_size;

    u8* data_in = buffer + command_header_size;
    u8* const data_in_end = data_in + command_length;

    // Helper to check that iterating over data n times is safe,
    // i.e. *data++ at most lead to data.end()
    auto validate_data_in_out = [&](u32 n_in, u32 n_out, std::string_view command) -> bool {
      if (data_in + n_in > data_in_end)
        ERROR_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: data_in overflow in {}", command);
      else if (u64{data_offset} + n_out > usable_out_size)
        ERROR_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: data_out overflow in {}", command);
      else
        return true;
      ERROR_LOG_FMT(SERIALINTERFACE_AMBB,
                    "Overflow details:\n"
                    " - data_in(begin={}, current={}, end={}, n_in={})\n"
                    " - data_out(offset={}, size={}, n_out={})",
                    fmt::ptr(buffer + 2), fmt::ptr(data_in), fmt::ptr(data_in_end), n_in,
                    data_offset, data_out.size(), n_out);
      data_in = data_in_end;
      return false;
    };

    while (data_in < data_in_end)
    {
      const u32 gcam_command = *data_in++;
      switch (GCAMCommand(gcam_command))
      {
      case GCAMCommand::StatusSwitches:
      {
        if (!validate_data_in_out(1, 4, "StatusSwitches"))
          break;

        const u8 status = *data_in++;
        DEBUG_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x10, {:02x} (READ STATUS&SWITCHES)",
                      status);

        data_out[data_offset++] = gcam_command;
        data_out[data_offset++] = 0x2;

        // We read Test/Service from the JVS-I/O SwitchesInput instead
        //
        // const GCPadStatus pad_status = Pad::GetStatus(ISIDevice::m_device_number);
        // baseboard test/service switches
        // if (pad_status.button & PAD_BUTTON_Y)	// Test
        //  dip_switch_0 &= ~0x80;
        // if (pad_status.button & PAD_BUTTON_X)	// Service
        //  dip_switch_0 &= ~0x40;

        // Horizontal Scanning Frequency switch
        // Required for F-Zero AX booting via Sega Boot
        if (AMMediaboard::GetGameType() == FZeroAX || AMMediaboard::GetGameType() == FZeroAXMonster)
        {
          m_dip_switch_0 &= ~0x20;
        }

        // Disable camera in MKGP1/2
        if (AMMediaboard::GetGameType() == MarioKartGP ||
            AMMediaboard::GetGameType() == MarioKartGP2)
        {
          m_dip_switch_0 &= ~0x10;
        }

        data_out[data_offset++] = m_dip_switch_0;
        data_out[data_offset++] = m_dip_switch_1;
        break;
      }
      case GCAMCommand::SerialNumber:
      {
        if (!validate_data_in_out(1, 18, "SerialNumber"))
          break;

        NOTICE_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x11, {:02x} (READ SERIAL NR)",
                       *data_in);
        data_in++;

        data_out[data_offset++] = gcam_command;
        data_out[data_offset++] = 16;
        memcpy(data_out.data() + data_offset, "AADE-01B98394904", 16);

        data_offset += 16;
        break;
      }
      case GCAMCommand::Unknown_12:
        if (!validate_data_in_out(2, 2, "Unknown_12"))
          break;

        NOTICE_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x12, {:02x} {:02x}", data_in[0],
                       data_in[1]);

        data_out[data_offset++] = gcam_command;
        data_out[data_offset++] = 0x00;

        data_in += 2;
        break;
      case GCAMCommand::Unknown_14:
        if (!validate_data_in_out(2, 2, "Unknown_14"))
          break;

        NOTICE_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x14, {:02x} {:02x}", data_in[0],
                       data_in[1]);

        data_out[data_offset++] = gcam_command;
        data_out[data_offset++] = 0x00;

        data_in += 2;
        break;
      case GCAMCommand::FirmVersion:
        if (!validate_data_in_out(1, 4, "FirmVersion"))
          break;

        NOTICE_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x15, {:02x} (READ FIRM VERSION)",
                       *data_in);
        data_in++;

        data_out[data_offset++] = gcam_command;
        data_out[data_offset++] = 0x02;
        // 00.26
        data_out[data_offset++] = 0x00;
        data_out[data_offset++] = 0x26;
        break;
      case GCAMCommand::FPGAVersion:
        if (!validate_data_in_out(1, 4, "FPGAVersion"))
          break;

        NOTICE_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x16, {:02x} (READ FPGA VERSION)",
                       *data_in);
        data_in++;

        data_out[data_offset++] = gcam_command;
        data_out[data_offset++] = 0x02;
        // 07.06
        data_out[data_offset++] = 0x07;
        data_out[data_offset++] = 0x06;
        break;
      case GCAMCommand::RegionSettings:
      {
        if (!validate_data_in_out(5, 0x16, "RegionSettings"))
          break;

        // Used by SegaBoot for region checks (dev mode skips this check)
        // In some games this also controls the displayed language
        NOTICE_LOG_FMT(SERIALINTERFACE_AMBB,
                       "GC-AM: Command 0x1F, {:02x} {:02x} {:02x} {:02x} {:02x} (REGION)",
                       data_in[0], data_in[1], data_in[2], data_in[3], data_in[4]);

        data_out[data_offset++] = gcam_command;
        data_out[data_offset++] = 0x14;

        for (int i = 0; i < 0x14; ++i)
          data_out[data_offset++] = s_region_flags[i];

        data_in += 5;
      }
      break;
      // No reply
      // Note: Always sends three bytes even though size is set to two
      case GCAMCommand::Unknown_21:
      {
        if (!validate_data_in_out(4, 0, "Unknown_21"))
          break;

        DEBUG_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x21, {:02x}, {:02x}, {:02x}, {:02x}",
                      data_in[0], data_in[1], data_in[2], data_in[3]);
        data_in += 4;
      }
      break;
      // No reply
      // Note: Always sends six bytes
      case GCAMCommand::Unknown_22:
      {
        if (!validate_data_in_out(7, 0, "Unknown_22"))
          break;

        DEBUG_LOG_FMT(SERIALINTERFACE_AMBB,
                      "GC-AM: Command 0x22, {:02x}, {:02x}, {:02x}, {:02x}, {:02x}, {:02x}, {:02x}",
                      data_in[0], data_in[1], data_in[2], data_in[3], data_in[4], data_in[5],
                      data_in[6]);

        const u32 in_size = data_in[0] + 1;
        if (!validate_data_in_out(in_size, 0, "Unknown_22"))
          break;
        data_in += in_size;
      }
      break;
      case GCAMCommand::Unknown_23:
        if (!validate_data_in_out(2, 2, "Unknown_23"))
          break;

        DEBUG_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x23, {:02x} {:02x}", data_in[0],
                      data_in[1]);

        data_out[data_offset++] = gcam_command;
        data_out[data_offset++] = 0x00;

        data_in += 2;
        break;
      case GCAMCommand::Unknown_24:
        if (!validate_data_in_out(2, 2, "Unknown_24"))
          break;

        DEBUG_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x24, {:02x} {:02x}", data_in[0],
                      data_in[1]);

        data_out[data_offset++] = gcam_command;
        data_out[data_offset++] = 0x00;

        data_in += 2;
        break;
      case GCAMCommand::SerialA:
      {
        if (!validate_data_in_out(1, 0, "SerialA"))
          break;

        const u32 length = *data_in++;
        if (length)
        {
          if (!validate_data_in_out(length, 0, "SerialA"))
            break;

          if (m_serial_device_a != nullptr)
          {
            m_serial_device_a->WriteRxBytes({data_in, length});
            data_in += length;
            break;
          }

          INFO_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x31, length=0x{:02x}, hexdump:\n{}",
                       length, HexDump(data_in, length));

          // Serial - Wheel
          if (AMMediaboard::GetGameType() == MarioKartGP ||
              AMMediaboard::GetGameType() == MarioKartGP2)
          {
            if (!validate_data_in_out(10, 2, "SerialA (Wheel)"))
              break;

            INFO_LOG_FMT(SERIALINTERFACE_AMBB,
                         "GC-AM: Command 0x31, (WHEEL) {:02x}{:02x} {:02x}{:02x} {:02x} {:02x} "
                         "{:02x} {:02x} {:02x} {:02x}",
                         data_in[0], data_in[1], data_in[2], data_in[3], data_in[4], data_in[5],
                         data_in[6], data_in[7], data_in[8], data_in[9]);

            data_out[data_offset++] = gcam_command;
            data_out[data_offset++] = 0x03;

            switch (m_wheel_init)
            {
            case 0:
              if (!validate_data_in_out(0, 3, "SerialA (Wheel)"))
                break;
              data_out[data_offset++] = 'E';  // Error
              data_out[data_offset++] = '0';
              data_out[data_offset++] = '0';
              m_wheel_init++;
              break;
            case 1:
              if (!validate_data_in_out(0, 3, "SerialA (Wheel)"))
                break;
              data_out[data_offset++] = 'C';  // Power Off
              data_out[data_offset++] = '0';
              data_out[data_offset++] = '6';
              // Only turn on when a wheel is connected
              if (serial_interface.GetDeviceType(1) == SerialInterface::SIDEVICE_GC_STEERING)
              {
                m_wheel_init++;
              }
              break;
            case 2:
              if (!validate_data_in_out(0, 3, "SerialA (Wheel)"))
                break;
              data_out[data_offset++] = 'C';  // Power On
              data_out[data_offset++] = '0';
              data_out[data_offset++] = '1';
              break;
            default:
              break;
            }

            // u16 CenteringForce= ptr(6);
            // u16 FrictionForce = ptr(8);
            // u16 Roll          = ptr(10);
            if (!validate_data_in_out(length, 0, "SerialA (Wheel)"))
              break;
            data_in += length;
            break;
          }
        }

        u32 command_offset = 0;
        while (command_offset < length)
        {
          // All commands are OR'd with 0x80
          // Last byte is checksum which we don't care about
          if (!validate_data_in_out(command_offset + 4, 0, "SerialA"))
            break;
          const u32 serial_command = Common::swap32(data_in + command_offset) ^ 0x80000000;

          if (AMMediaboard::GetGameType() == FZeroAX ||
              AMMediaboard::GetGameType() == FZeroAXMonster)
          {
            INFO_LOG_FMT(SERIALINTERFACE_AMBB,
                         "GC-AM: Command 0x31 (MOTOR) Length:{:02x} Command:{:04x}({:02x})", length,
                         (serial_command >> 8) & 0xFFFF, serial_command >> 24);
          }
          else
          {
            INFO_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x31 (SERIAL) Command:{:06x}",
                         serial_command);

            if (serial_command == 0x801000)
            {
              if (!validate_data_in_out(0, 4, "SerialA"))
                break;
              data_out[data_offset++] = 0x31;
              data_out[data_offset++] = 0x02;
              data_out[data_offset++] = 0xFF;
              data_out[data_offset++] = 0x01;
            }
          }

          command_offset += sizeof(u32);

          if (AMMediaboard::GetGameType() == FZeroAX ||
              AMMediaboard::GetGameType() == FZeroAXMonster)
          {
            if (command_offset + 5 >= std::size(m_motor_reply))
            {
              ERROR_LOG_FMT(
                  SERIALINTERFACE_AMBB,
                  "GC-AM: Command 0x31 (MOTOR) overflow: offset={} >= motor_reply_size={}",
                  command_offset + 5, std::size(m_motor_reply));
              data_in = data_in_end;
              break;
            }

            // Status
            m_motor_reply[command_offset + 2] = 0;
            m_motor_reply[command_offset + 3] = 0;

            // Error
            m_motor_reply[command_offset + 4] = 0;

            switch (serial_command >> 24)
            {
            case 0:
            case 1:  // Set Maximum?
            case 2:
              break;

            // 0x00-0x40: left
            // 0x40-0x80: right
            case 4:  // Move Steering Wheel
              // Left
              if (serial_command & 0x010000)
              {
                m_motor_force_y = -((s16)serial_command & 0xFF00);
              }
              else  // Right
              {
                m_motor_force_y = (serial_command - 0x4000) & 0xFF00;
              }

              m_motor_force_y *= 2;

              // FFB
              if (m_motor_init == 2)
              {
                if (serial_interface.GetDeviceType(1) == SerialInterface::SIDEVICE_GC_STEERING)
                {
                  const GCPadStatus pad_status = Pad::GetStatus(1);
                  if (pad_status.isConnected)
                  {
                    const ControlState mapped_strength = (double)(m_motor_force_y >> 8) / 127.f;

                    Pad::Rumble(1, mapped_strength);
                    INFO_LOG_FMT(SERIALINTERFACE_AMBB,
                                 "GC-AM: Command 0x31 (MOTOR) mapped_strength:{}", mapped_strength);
                  }
                }
              }
              break;
            case 6:  // nice
            case 9:
            default:
              break;
            // Switch back to normal controls
            case 7:
              m_motor_init = 2;
              break;
            // Reset
            case 0x7F:
              m_motor_init = 1;
              memset(m_motor_reply, 0, sizeof(m_motor_reply));
              break;
            }

            // Checksum
            m_motor_reply[command_offset + 5] = m_motor_reply[command_offset + 2] ^
                                                m_motor_reply[command_offset + 3] ^
                                                m_motor_reply[command_offset + 4];
          }
        }

        if (length == 0)
        {
          if (!validate_data_in_out(length, 2, "SerialA"))
            break;
          data_out[data_offset++] = gcam_command;
          data_out[data_offset++] = 0x00;
        }
        else
        {
          if (m_motor_init)
          {
            // Motor
            m_motor_reply[0] = gcam_command;
            m_motor_reply[1] = length;  // Same out as in size

            const u32 reply_size = m_motor_reply[1] + 2;
            if (!validate_data_in_out(length, reply_size, "SerialA"))
              break;

            if (reply_size > sizeof(m_motor_reply))
            {
              ERROR_LOG_FMT(SERIALINTERFACE_AMBB,
                            "GC-AM: Command SerialA, reply_size={} too big for m_motor_reply",
                            reply_size);
              data_in = data_in_end;
              break;
            }
            memcpy(data_out.data() + data_offset, m_motor_reply, reply_size);
            data_offset += reply_size;
          }
        }

        if (!validate_data_in_out(length, 0, "SerialA"))
        {
          break;
        }
        data_in += length;
        break;
      }
      case GCAMCommand::SerialB:
      {
        if (!validate_data_in_out(1, 0, "SerialB"))
          break;
        const u32 in_length = *data_in++;

        if (!validate_data_in_out(in_length, 0, "SerialB"))
          break;

        if (m_serial_device_b != nullptr)
        {
          m_serial_device_b->WriteRxBytes({data_in, in_length});
        }

        data_in += in_length;

        break;
      }
      case GCAMCommand::JVSIOA:
      case GCAMCommand::JVSIOB:
      {
        if (!validate_data_in_out(4, 0, "JVSIO"))
          break;

        JVSIOMessage message;

        const u8* const frame = &data_in[0];
        const u8 nr_bytes = frame[3];  // Byte after E0 xx
        u32 frame_len = nr_bytes + 3;  // Header(2) + length byte + payload + checksum

        u8 jvs_buf[0x80];

        frame_len = std::min<u32>(frame_len, sizeof(jvs_buf));

        if (!validate_data_in_out(frame_len, 0, "JVSIO"))
          break;
        DEBUG_LOG_FMT(SERIALINTERFACE_JVSIO, "GC-AM: Command {:02x} (JVS IO), hexdump:\n{}",
                      gcam_command, HexDump(data_in, frame_len));

        memcpy(jvs_buf, frame, frame_len);

        // Extract node and payload pointers
        u8 node = jvs_buf[2];
        u8* jvs_io = jvs_buf + 4;                 // First payload byte
        u8* const jvs_end = jvs_buf + frame_len;  // One byte before checksum
        u8* const jvs_begin = jvs_io;

        message.Start(0);
        message.AddData(1);

        // Helper to check that iterating over jvs_io n times is safe,
        // i.e. *jvs_io++ at most lead to jvs_end
        auto validate_jvs_io = [&](u32 n, std::string_view command) -> bool {
          if (jvs_io + n > jvs_end)
            ERROR_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: overflow in {}", command);
          else
            return true;
          ERROR_LOG_FMT(SERIALINTERFACE_JVSIO,
                        "Overflow details:\n"
                        " - jvs_io(begin={}, current={}, end={}, n={})\n"
                        " - delay={}, node={}\n"
                        " - frame(begin={}, len={})",
                        fmt::ptr(jvs_begin), fmt::ptr(jvs_io), fmt::ptr(jvs_end), n, m_delay, node,
                        fmt::ptr(frame), frame_len);
          jvs_io = jvs_end;
          return false;
        };

        // Now iterate over the payload
        while (jvs_io < jvs_end)
        {
          const u8 jvsio_command = *jvs_io++;
          DEBUG_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO:node={}, command={:02x}", node,
                        jvsio_command);

          switch (JVSIOCommand(jvsio_command))
          {
          case JVSIOCommand::IOID:
            message.AddData(StatusOkay);
            switch (AMMediaboard::GetGameType())
            {
            case FZeroAX:
              // Specific version that enables DX mode on AX machines, all this does is enable the
              // motion of a chair
              message.AddData("SEGA ENTERPRISES,LTD.;837-13844-01 I/O CNTL BD2 ;");
              break;
            case FZeroAXMonster:
            case MarioKartGP:
            case MarioKartGP2:
            default:
              message.AddData("namco ltd.;FCA-1;Ver1.01;JPN,Multipurpose + Rotary Encoder");
              break;
            case VirtuaStriker3:
            case VirtuaStriker4:
            case VirtuaStriker4_2006:
              message.AddData("SEGA ENTERPRISES,LTD.;I/O BD JVS;837-13551;Ver1.00");
              break;
            }
            NOTICE_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Command 0x10, BoardID");
            message.AddData((u32)0);
            break;
          case JVSIOCommand::CommandRevision:
            message.AddData(StatusOkay);
            message.AddData(0x11);
            NOTICE_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Command 0x11, CommandRevision");
            break;
          case JVSIOCommand::JVRevision:
            message.AddData(StatusOkay);
            message.AddData(0x20);
            NOTICE_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Command 0x12, JVRevision");
            break;
          case JVSIOCommand::CommunicationVersion:
            message.AddData(StatusOkay);
            message.AddData(0x10);
            NOTICE_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Command 0x13, CommunicationVersion");
            break;

          // Slave features:
          //
          // Inputs:
          // 0x01: Switch input:  players,  buttons
          // 0x02: Coin input:    slots
          // 0x03: Analog input:  channels, bits
          // 0x04: Rotary input: channels
          // 0x05: Keycode input: 0,0,0 ?
          // 0x06: Screen position input: X bits, Y bits, channels
          //
          // Outputs:
          // 0x10: Card system: slots
          // 0x11: Medal hopper: channels
          // 0x12: GPO-out: slots
          // 0x13: Analog output: channels
          // 0x14: Character output: width, height, type
          // 0x15: Backup
          case JVSIOCommand::CheckFunctionality:
            message.AddData(StatusOkay);
            switch (AMMediaboard::GetGameType())
            {
            case FZeroAX:
            case FZeroAXMonster:
              // 2 Player (12bit) (p2=paddles), 1 Coin slot, 6 Analog-in
              // message.AddData((void *)"\x01\x02\x0C\x00", 4);
              // message.AddData((void *)"\x02\x01\x00\x00", 4);
              // message.AddData((void *)"\x03\x06\x00\x00", 4);
              // message.AddData((void *)"\x00\x00\x00\x00", 4);
              //
              // DX Version: 2 Player (22bit) (p2=paddles), 2 Coin slot, 8 Analog-in,
              // 22 Driver-out
              message.AddData("\x01\x02\x12\x00", 4);
              message.AddData("\x02\x02\x00\x00", 4);
              message.AddData("\x03\x08\x0A\x00", 4);
              message.AddData("\x12\x16\x00\x00", 4);
              message.AddData("\x00\x00\x00\x00", 4);
              break;
            case VirtuaStriker3:
              // 2 Player (13bit), 2 Coin slot, 4 Analog-in, 8 Driver-out
              message.AddData("\x01\x02\x0D\x00", 4);
              message.AddData("\x02\x02\x00\x00", 4);
              message.AddData("\x12\x08\x00\x00", 4);
              message.AddData("\x00\x00\x00\x00", 4);
              break;
            case GekitouProYakyuu:
              // 2 Player (13bit), 2 Coin slot, 4 Analog-in, 8 Driver-out
              message.AddData("\x01\x02\x0D\x00", 4);
              message.AddData("\x02\x02\x00\x00", 4);
              message.AddData("\x03\x04\x00\x00", 4);
              message.AddData("\x12\x08\x00\x00", 4);
              message.AddData("\x00\x00\x00\x00", 4);
              break;
            case VirtuaStriker4:
            case VirtuaStriker4_2006:
              // 2 Player (13bit), 1 Coin slot, 4 Analog-in, 22 Driver-out
              message.AddData("\x01\x02\x0D\x00", 4);
              message.AddData("\x02\x01\x00\x00", 4);
              message.AddData("\x03\x04\x00\x00", 4);
              message.AddData("\x12\x16\x00\x00", 4);
              message.AddData("\x00\x00\x00\x00", 4);
              break;
            case KeyOfAvalon:
              // 1 Player (15bit), 1 Coin slot, 3 Analog-in, Touch, 1 Driver-out
              // (Unconfirmed)
              message.AddData("\x01\x01\x0F\x00", 4);
              message.AddData("\x02\x01\x00\x00", 4);
              message.AddData("\x03\x03\x00\x00", 4);
              message.AddData("\x06\x10\x10\x01", 4);
              message.AddData("\x12\x01\x00\x00", 4);
              message.AddData("\x00\x00\x00\x00", 4);
              break;
            case MarioKartGP:
            case MarioKartGP2:
            default:
              // 1 Player (15bit), 1 Coin slot, 3 Analog-in, 1 Driver-out
              message.AddData("\x01\x01\x0F\x00", 4);
              message.AddData("\x02\x01\x00\x00", 4);
              message.AddData("\x03\x03\x00\x00", 4);
              message.AddData("\x12\x01\x00\x00", 4);
              message.AddData("\x00\x00\x00\x00", 4);
              break;
            }
            NOTICE_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Command 0x14, CheckFunctionality");
            break;
          case JVSIOCommand::MainID:
          {
            const u8* const main_id = jvs_io;
            while (jvs_io < jvs_end && *jvs_io++)
            {
            }
            if (main_id < jvs_io)
            {
              DEBUG_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Command MainId:\n{}",
                            HexDump(main_id, jvs_io - main_id));
            }
            message.AddData(StatusOkay);
            break;
          }
          case JVSIOCommand::SwitchesInput:
          {
            if (!validate_jvs_io(2, "SwitchesInput"))
              break;
            const u32 player_count = *jvs_io++;
            const u32 player_byte_count = *jvs_io++;

            DEBUG_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO:  Command 0x20, SwitchInputs: {} {}",
                          player_count, player_byte_count);

            message.AddData(StatusOkay);

            GCPadStatus pad_status = Pad::GetStatus(0);

            // Test button
            if (pad_status.switches & SWITCH_TEST)
            {
              // Trying to access the test menu without SegaBoot present will cause a crash
              if (AMMediaboard::GetTestMenu())
              {
                message.AddData(0x80);
              }
              else
              {
                PanicAlertFmt("Test menu is disabled due to missing SegaBoot");
              }
            }
            else
            {
              message.AddData((u32)0x00);
            }

            for (u32 i = 0; i < player_count; ++i)
            {
              std::array<u8, 3> player_data{};

              const auto io_ports_player_data = m_io_ports.GetSwitchInputs(i);
              std::copy_n(io_ports_player_data.data(),
                          std::min(io_ports_player_data.size(), player_data.size()),
                          player_data.data());

              // Service button
              if (pad_status.switches & SWITCH_SERVICE)
                player_data[0] |= 0x40;

              switch (AMMediaboard::GetGameType())
              {
              // Controller configuration for F-Zero AX (DX)
              case FZeroAX:
                if (i == 0)
                {
                  if (m_fzdx_seatbelt)
                  {
                    player_data[0] |= 0x01;
                  }

                  // Start
                  if (pad_status.button & PAD_BUTTON_START)
                    player_data[0] |= 0x80;
                  // Boost
                  if (pad_status.button & PAD_BUTTON_A)
                    player_data[0] |= 0x02;
                  // View Change 1
                  if (pad_status.button & PAD_BUTTON_RIGHT)
                    player_data[0] |= 0x20;
                  // View Change 2
                  if (pad_status.button & PAD_BUTTON_LEFT)
                    player_data[0] |= 0x10;
                  // View Change 3
                  if (pad_status.button & PAD_BUTTON_UP)
                    player_data[0] |= 0x08;
                  // View Change 4
                  if (pad_status.button & PAD_BUTTON_DOWN)
                    player_data[0] |= 0x04;
                  player_data[1] = m_rx_reply & 0xF0;
                }
                else if (i == 1)
                {
                  //  Paddle left
                  if (pad_status.button & PAD_BUTTON_X)
                    player_data[0] |= 0x20;
                  //  Paddle right
                  if (pad_status.button & PAD_BUTTON_Y)
                    player_data[0] |= 0x10;

                  if (m_fzdx_motion_stop)
                  {
                    player_data[0] |= 2;
                  }
                  if (m_fzdx_sensor_right)
                  {
                    player_data[0] |= 4;
                  }
                  if (m_fzdx_sensor_left)
                  {
                    player_data[0] |= 8;
                  }

                  player_data[1] = m_rx_reply << 4;
                }
                break;
              // Controller configuration for F-Zero AX MonsterRide
              case FZeroAXMonster:
                if (i == 0)
                {
                  if (m_fzcc_sensor)
                  {
                    player_data[0] |= 0x01;
                  }

                  // Start
                  if (pad_status.button & PAD_BUTTON_START)
                    player_data[0] |= 0x80;
                  // Boost
                  if (pad_status.button & PAD_BUTTON_A)
                    player_data[0] |= 0x02;
                  // View Change 1
                  if (pad_status.button & PAD_BUTTON_RIGHT)
                    player_data[0] |= 0x20;
                  // View Change 2
                  if (pad_status.button & PAD_BUTTON_LEFT)
                    player_data[0] |= 0x10;
                  // View Change 3
                  if (pad_status.button & PAD_BUTTON_UP)
                    player_data[0] |= 0x08;
                  // View Change 4
                  if (pad_status.button & PAD_BUTTON_DOWN)
                    player_data[0] |= 0x04;

                  player_data[1] = m_rx_reply & 0xF0;
                }
                else if (i == 1)
                {
                  //  Paddle left
                  if (pad_status.button & PAD_BUTTON_X)
                    player_data[0] |= 0x20;
                  //  Paddle right
                  if (pad_status.button & PAD_BUTTON_Y)
                    player_data[0] |= 0x10;

                  if (m_fzcc_seatbelt)
                  {
                    player_data[0] |= 2;
                  }
                  if (m_fzcc_service)
                  {
                    player_data[0] |= 4;
                  }
                  if (m_fzcc_emergency)
                  {
                    player_data[0] |= 8;
                  }
                }
                break;
              // Controller configuration for Virtua Striker 3 games
              case VirtuaStriker3:
                pad_status = Pad::GetStatus(i);
                // Start
                if (pad_status.button & PAD_BUTTON_START)
                  player_data[0] |= 0x80;
                // Shoot
                if (pad_status.button & PAD_BUTTON_B)
                  player_data[0] |= 0x01;
                // Short Pass
                if (pad_status.button & PAD_BUTTON_A)
                  player_data[1] |= 0x80;
                // Long Pass
                if (pad_status.button & PAD_BUTTON_X)
                  player_data[0] |= 0x02;
                // Left
                if (pad_status.button & PAD_BUTTON_LEFT)
                  player_data[0] |= 0x08;
                // Up
                if (pad_status.button & PAD_BUTTON_UP)
                  player_data[0] |= 0x20;
                // Right
                if (pad_status.button & PAD_BUTTON_RIGHT)
                  player_data[0] |= 0x04;
                // Down
                if (pad_status.button & PAD_BUTTON_DOWN)
                  player_data[0] |= 0x10;
                break;
              // Controller configuration for Virtua Striker 4 games
              case VirtuaStriker4:
              case VirtuaStriker4_2006:
              {
                pad_status = Pad::GetStatus(i);
                // Start
                if (pad_status.button & PAD_BUTTON_START)
                  player_data[0] |= 0x80;
                // Long Pass
                if (pad_status.button & PAD_BUTTON_X)
                  player_data[0] |= 0x01;
                // Short Pass
                if (pad_status.button & PAD_BUTTON_A)
                  player_data[0] |= 0x02;
                // Shoot
                if (pad_status.button & PAD_BUTTON_B)
                  player_data[1] |= 0x80;
                // Dash
                if (pad_status.button & PAD_BUTTON_Y)
                  player_data[1] |= 0x40;
                // Tactics (U)
                if (pad_status.button & PAD_BUTTON_LEFT)
                  player_data[0] |= 0x20;
                // Tactics (M)
                if (pad_status.button & PAD_BUTTON_UP)
                  player_data[0] |= 0x08;
                // Tactics (D)
                if (pad_status.button & PAD_BUTTON_RIGHT)
                  player_data[0] |= 0x04;
              }
              break;
              // Controller configuration for Gekitou Pro Yakyuu
              case GekitouProYakyuu:
                pad_status = Pad::GetStatus(i);
                // Start
                if (pad_status.button & PAD_BUTTON_START)
                  player_data[0] |= 0x80;
                //  A
                if (pad_status.button & PAD_BUTTON_B)
                  player_data[0] |= 0x01;
                //  B
                if (pad_status.button & PAD_BUTTON_A)
                  player_data[0] |= 0x02;
                //  Gekitou
                if (pad_status.button & PAD_TRIGGER_L)
                  player_data[1] |= 0x80;
                // Left
                if (pad_status.button & PAD_BUTTON_LEFT)
                  player_data[0] |= 0x08;
                // Up
                if (pad_status.button & PAD_BUTTON_UP)
                  player_data[0] |= 0x20;
                // Right
                if (pad_status.button & PAD_BUTTON_RIGHT)
                  player_data[0] |= 0x04;
                // Down
                if (pad_status.button & PAD_BUTTON_DOWN)
                  player_data[0] |= 0x10;
                break;
              // Controller configuration for Mario Kart and other games
              default:
              case MarioKartGP:
              case MarioKartGP2:
              {
                // Start
                if (pad_status.button & PAD_BUTTON_START)
                  player_data[0] |= 0x80;
                // Item button
                if (pad_status.button & PAD_BUTTON_A)
                  player_data[1] |= 0x20;
                // VS-Cancel button
                if (pad_status.button & PAD_BUTTON_B)
                  player_data[1] |= 0x02;
              }
              break;
              case KeyOfAvalon:
              {
                // Debug On
                if (pad_status.button & PAD_BUTTON_START)
                  player_data[0] |= 0x80;
                // Switch 1
                if (pad_status.button & PAD_BUTTON_A)
                  player_data[0] |= 0x04;
                // Switch 2
                if (pad_status.button & PAD_BUTTON_B)
                  player_data[0] |= 0x08;
              }
              break;
              }

              if (player_byte_count > player_data.size())
              {
                WARN_LOG_FMT(SERIALINTERFACE_JVSIO,
                             "JVS-IO:  Command 0x20, SwitchInputs: invalid player_byte_count={}",
                             player_byte_count);
              }
              const u32 data_size = std::min(player_byte_count, u32(player_data.size()));
              for (u32 j = 0; j < data_size; ++j)
                message.AddData(player_data[j]);
            }
            break;
          }
          case JVSIOCommand::CoinInput:
          {
            if (!validate_jvs_io(1, "CoinInput"))
              break;
            const u32 slots = *jvs_io++;
            message.AddData(StatusOkay);
            static_assert(std::tuple_size<decltype(m_coin)>{} == 2 &&
                          std::tuple_size<decltype(m_coin_pressed)>{} == 2);
            if (slots > 2)
            {
              WARN_LOG_FMT(SERIALINTERFACE_JVSIO,
                           "JVS-IO: Command 0x21, CoinInput: invalid slots {}", slots);
            }
            const u32 max_slots = std::min(slots, 2u);
            for (u32 i = 0; i < max_slots; i++)
            {
              GCPadStatus pad_status = Pad::GetStatus(i);
              if ((pad_status.switches & SWITCH_COIN) && !m_coin_pressed[i])
              {
                m_coin[i]++;
              }
              m_coin_pressed[i] = pad_status.switches & SWITCH_COIN;
              message.AddData((m_coin[i] >> 8) & 0x3f);
              message.AddData(m_coin[i] & 0xff);
            }
            DEBUG_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Command 0x21, CoinInput: {}", slots);
            break;
          }
          case JVSIOCommand::AnalogInput:
          {
            if (!validate_jvs_io(1, "AnalogInput"))
              break;
            message.AddData(StatusOkay);

            const u32 analogs = *jvs_io++;
            GCPadStatus pad_status = Pad::GetStatus(0);

            DEBUG_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Command 0x22, AnalogInput: {}", analogs);

            switch (AMMediaboard::GetGameType())
            {
            case FZeroAX:
            case FZeroAXMonster:
              // Steering
              if (m_motor_init == 1)
              {
                if (m_motor_force_y > 0)
                {
                  message.AddData(0x80 - (m_motor_force_y >> 8));
                }
                else
                {
                  message.AddData((m_motor_force_y >> 8));
                }
                message.AddData((u8)0);

                message.AddData(pad_status.stickY);
                message.AddData((u8)0);
              }
              else
              {
                // The center for the Y axis is expected to be 78h this adjusts that
                message.AddData(pad_status.stickX - 12);
                message.AddData((u8)0);

                message.AddData(pad_status.stickY);
                message.AddData((u8)0);
              }

              // Unused
              message.AddData((u8)0);
              message.AddData((u8)0);
              message.AddData((u8)0);
              message.AddData((u8)0);

              // Gas
              message.AddData(pad_status.triggerRight);
              message.AddData((u8)0);

              // Brake
              message.AddData(pad_status.triggerLeft);
              message.AddData((u8)0);

              message.AddData((u8)0x80);  // Motion Stop
              message.AddData((u8)0);

              message.AddData((u8)0);
              message.AddData((u8)0);

              break;
            case VirtuaStriker4:
            case VirtuaStriker4_2006:
            {
              message.AddData(-pad_status.stickY);
              message.AddData((u8)0);
              message.AddData(pad_status.stickX);
              message.AddData((u8)0);

              pad_status = Pad::GetStatus(1);

              message.AddData(-pad_status.stickY);
              message.AddData((u8)0);
              message.AddData(pad_status.stickX);
              message.AddData((u8)0);
            }
            break;
            default:
            case MarioKartGP:
            case MarioKartGP2:
              // Steering
              message.AddData(pad_status.stickX);
              message.AddData((u8)0);

              // Gas
              message.AddData(pad_status.triggerRight);
              message.AddData((u8)0);

              // Brake
              message.AddData(pad_status.triggerLeft);
              message.AddData((u8)0);
              break;
            }
            break;
          }
          case JVSIOCommand::PositionInput:
          {
            if (!validate_jvs_io(1, "PositionInput"))
              break;
            const u32 channel = *jvs_io++;

            const GCPadStatus pad_status = Pad::GetStatus(0);

            if (pad_status.button & PAD_TRIGGER_R)
            {
              // Tap at center of screen (~320,240)
              message.AddData("\x01\x00\x8C\x01\x95",
                              5);  // X=320 (0x0140), Y=240 (0x00F0)
            }
            else
            {
              message.AddData("\x01\xFF\xFF\xFF\xFF", 5);
            }

            DEBUG_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Command 0x25, PositionInput:{}", channel);
            break;
          }
          case JVSIOCommand::CoinSubOutput:
          {
            if (!validate_jvs_io(3, "CoinSubOutput"))
              break;
            const u32 slot = *jvs_io++;
            const u8 coinh = *jvs_io++;
            const u8 coinl = *jvs_io++;

            if (slot < m_coin.size())
            {
              m_coin[slot] -= (coinh << 8) | coinl;
            }
            else
            {
              WARN_LOG_FMT(SERIALINTERFACE_JVSIO,
                           "JVS-IO: Command 0x30, CoinSubOutput: invalid slot {}", slot);
            }

            message.AddData(StatusOkay);
            DEBUG_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Command 0x30, CoinSubOutput: {}", slot);
            break;
          }
          case JVSIOCommand::GeneralDriverOutput:
          {
            if (!validate_jvs_io(1, "GeneralDriverOutput"))
              break;
            const u32 bytes = *jvs_io++;

            if (!validate_jvs_io(bytes, "GeneralDriverOutput"))
              break;

            message.AddData(StatusOkay);

            if (bytes)
            {
              m_io_ports.SetGenericOutputs({jvs_io, bytes});

              DEBUG_LOG_FMT(SERIALINTERFACE_JVSIO,
                            "JVS-IO: GPO: delay=0x{:02x}, rx_reply=0x{:02x},"
                            " bytes={}, buffer:\n{}",
                            m_delay, m_rx_reply, bytes, HexDump(jvs_io, bytes));

              if ((AMMediaboard::GetGameType() == FZeroAX) && bytes >= 3)
              {
                // Handling of the motion seat used in F-Zero AXs DX version
                const u16 seat_state = Common::swap16(jvs_io + 1) >> 2;

                switch (seat_state)
                {
                case 0x70:
                  m_delay++;
                  if ((m_delay % 10) == 0)
                  {
                    m_rx_reply = 0xFB;
                  }
                  break;
                case 0xF0:
                  m_rx_reply = 0xF0;
                  break;
                default:
                case 0xA0:
                case 0x60:
                  break;
                }
              }

              jvs_io += bytes;
            }
            break;
          }
          case JVSIOCommand::CoinAddOutput:
          {
            if (!validate_jvs_io(3, "CoinAddOutput"))
              break;
            const u32 slot = *jvs_io++;
            const u8 coinh = *jvs_io++;
            const u8 coinl = *jvs_io++;

            m_coin[slot] += (coinh << 8) | coinl;

            message.AddData(StatusOkay);
            DEBUG_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Command 0x35, CoinAddOutput: {}", slot);
            break;
          }
          case JVSIOCommand::NAMCOCommand:
          {
            if (!validate_jvs_io(1, "NAMCOCommand"))
              break;
            const u32 namco_command = *jvs_io++;

            if (namco_command == 0x18)
            {
              if (!validate_jvs_io(4, "NAMCOCommand(0x18) / ID check"))
                break;
              // ID check
              jvs_io += 4;
              message.AddData(StatusOkay);
              message.AddData(0xff);
            }
            else
            {
              message.AddData(StatusOkay);
              ERROR_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Unknown:{:02x}", namco_command);
            }
            break;
          }
          case JVSIOCommand::Reset:
            if (!validate_jvs_io(1, "Reset"))
              break;
            if (*jvs_io++ == 0xD9)
            {
              NOTICE_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Command 0xF0, Reset");
              m_delay = 0;
              m_wheel_init = 0;
            }
            message.AddData(StatusOkay);

            m_dip_switch_1 |= 1;
            break;
          case JVSIOCommand::SetAddress:
            if (!validate_jvs_io(1, "SetAddress"))
              break;
            node = *jvs_io++;
            NOTICE_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Command 0xF1, SetAddress: node={}",
                           node);
            message.AddData(node == 1);
            m_dip_switch_1 &= ~1u;
            break;
          default:
            ERROR_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Unhandled: node={}, command={:02x}", node,
                          jvsio_command);
            break;
          }
        }

        message.End();

        if (!validate_data_in_out(0, 2, "JVSIO"))
          break;
        data_out[data_offset++] = gcam_command;

        const u8* buf = message.m_message.data();
        const u32 len = message.m_pointer;
        data_out[data_offset++] = len;
        const u32 in_size = frame[0] + 1;

        if (!validate_data_in_out(in_size, len, "JVSIO"))
          break;
        for (u32 i = 0; i < len; ++i)
          data_out[data_offset++] = buf[i];

        data_in += in_size;
        break;
      }
      case GCAMCommand::Unknown_60:
      {
        if (!validate_data_in_out(3, 0, "Unknown_60"))
          break;

        NOTICE_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x60, {:02x} {:02x} {:02x}",
                       data_in[0], data_in[1], data_in[2]);

        const u32 in_size = data_in[0] + 1;
        if (!validate_data_in_out(in_size, 0, "Unknown_60"))
          break;
        data_in += in_size;
      }
      break;
      default:
        if (!validate_data_in_out(5, 0, fmt::format("Unknown_{}", gcam_command)))
          break;
        ERROR_LOG_FMT(SERIALINTERFACE_AMBB,
                      "GC-AM: Command {:02x} (unknown) {:02x} {:02x} {:02x} {:02x} {:02x}",
                      gcam_command, data_in[0], data_in[1], data_in[2], data_in[3], data_in[4]);
        break;
      }
    }

    // Update attached serial devices and read data into our response buffer.
    // This is done regardless of the game having just sent a SerialA/B write.

    const auto process_serial_device = [&](Triforce::SerialDevice* serial_device, GCAMCommand cmd) {
      if (serial_device == nullptr)
        return;

      serial_device->Update();

      const auto out_length =
          std::min(u32(serial_device->GetTxByteCount()), SERIAL_PORT_MAX_READ_SIZE);

      if (out_length == 0)
        return;

      // Also accounting for the 2-byte header.
      if (!validate_data_in_out(0, out_length + 2, "SerialDevice"))
        return;

      // Write the 2-byte header.
      data_out[data_offset++] = cmd;
      data_out[data_offset++] = u8(out_length);

      const auto out_span = std::span{data_out}.subspan(data_offset, out_length);

      serial_device->TakeTxBytes(out_span);

      data_offset += out_length;
    };

    m_io_ports.Update();

    process_serial_device(m_serial_device_a.get(), GCAMCommand::SerialA);
    process_serial_device(m_serial_device_b.get(), GCAMCommand::SerialB);

    data_out[0] = 0x01;                                // Status code ?
    data_out[1] = data_offset - response_header_size;  // Length

    // Zero-fill the rest of the buffer.
    std::fill(data_out.data() + data_offset, data_out.data() + usable_out_size, 0x00);

    // Compute the checksum (bitwise-xor of additive sum).
    const u8 checksum = ~std::accumulate(data_out.data(), data_out.data() + data_offset, u8{});
    data_out.back() = checksum;

    DEBUG_LOG_FMT(SERIALINTERFACE_AMBB, "Command response: {}",
                  HexDump(data_out.data(), data_out.size()));

    // Switch to next buffer and return it.
    if (++m_current_response_buffer_index == m_response_buffers.size())
      m_current_response_buffer_index = 0;

    auto& response_buffer = m_response_buffers[m_current_response_buffer_index];

    const bool buffer_has_data = response_buffer.front() != 0x00;
    if (!buffer_has_data)
      return 0;

    std::ranges::copy(response_buffer, buffer);
    return int(response_buffer.size());
  }
  default:
  {
    ERROR_LOG_FMT(SERIALINTERFACE, "Unknown SI command (0x{:08x})", (u32)bb_command);
    PanicAlertFmt("SI: Unknown command");
    break;
  }
  }

  // No response.
  return -1;
}

DataResponse CSIDevice_AMBaseboard::GetData(u32&, u32&)
{
  return DataResponse::NoData;
}

void CSIDevice_AMBaseboard::SendCommand(u32 command, u8 poll)
{
  WARN_LOG_FMT(SERIALINTERFACE_AMBB, "Unhandled SendCommand(0x{:08x}, 0x{:02x})", command, poll);
}

void CSIDevice_AMBaseboard::DoState(PointerWrap& p)
{
  p.Do(m_response_buffers);
  p.Do(m_current_response_buffer_index);

  p.Do(m_coin);
  p.Do(m_coin_pressed);

  m_io_ports.DoState(p);

  // Serial A
  if (m_serial_device_a != nullptr)
    m_serial_device_a->DoState(p);

  // Serial B
  if (m_serial_device_b != nullptr)
    m_serial_device_b->DoState(p);

  // Serial
  p.Do(m_wheel_init);

  p.Do(m_motor_init);
  p.Do(m_motor_reply);
  p.Do(m_motor_force_y);

  // F-Zero AX (DX)
  p.Do(m_fzdx_seatbelt);
  p.Do(m_fzdx_motion_stop);
  p.Do(m_fzdx_sensor_right);
  p.Do(m_fzdx_sensor_left);
  p.Do(m_rx_reply);

  // F-Zero AX (CyCraft)
  p.Do(m_fzcc_seatbelt);
  p.Do(m_fzcc_sensor);
  p.Do(m_fzcc_emergency);
  p.Do(m_fzcc_service);

  p.Do(m_dip_switch_1);
  p.Do(m_dip_switch_0);

  p.Do(m_delay);
}

}  // namespace SerialInterface
