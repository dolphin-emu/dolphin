// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/SI/SI_DeviceAMBaseboard.h"

#include <algorithm>
#include <numeric>
#include <string>

#include <fmt/format.h>

#include "Common/Buffer.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/Swap.h"

#include "Core/Boot/Boot.h"
#include "Core/BootManager.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/DVD/AMMediaboard.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/EXI/EXI.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/HW/SI/SI_DeviceGCController.h"
#include "Core/HW/SystemTimers.h"
#include "Core/Movie.h"
#include "Core/NetPlayProto.h"
#include "Core/System.h"

#include "InputCommon/GCPadStatus.h"

namespace SerialInterface
{
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

static constexpr u8 CheckSumXOR(const u8* data, u32 length)
{
  return std::accumulate(data, data + length, u8{}, std::bit_xor());
}

static constexpr char s_cdr_program_version[] = {"           Version 1.22,2003/09/19,171-8213B"};
static constexpr char s_cdr_boot_version[] = {"           Version 1.04,2003/06/17,171-8213B"};
static constexpr u8 s_cdr_card_data[] = {
    0x00, 0x6E, 0x00, 0x00, 0x01, 0x00, 0x00, 0x06, 0x00, 0x00, 0x07, 0x00, 0x00, 0x0B,
    0x00, 0x00, 0x0E, 0x00, 0x00, 0x10, 0x00, 0x00, 0x17, 0x00, 0x00, 0x19, 0x00, 0x00,
    0x1A, 0x00, 0x00, 0x1B, 0x00, 0x00, 0x1D, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x20, 0x00,
    0x00, 0x22, 0x00, 0x00, 0x23, 0x00, 0x00, 0x24, 0x00, 0x00, 0x27, 0x00, 0x00, 0x28,
    0x00, 0x00, 0x2C, 0x00, 0x00, 0x2F, 0x00, 0x00, 0x34, 0x00, 0x00, 0x35, 0x00, 0x00,
    0x37, 0x00, 0x00, 0x38, 0x00, 0x00, 0x39, 0x00, 0x00, 0x3D, 0x00};

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
  // Card ID
  m_ic_card_data[0x20] = 0x95;
  m_ic_card_data[0x21] = 0x71;

  if (AMMediaboard::GetGameType() == KeyOfAvalon)
  {
    m_ic_card_data[0x22] = 0x26;
    m_ic_card_data[0x23] = 0x40;
  }
  else if (AMMediaboard::GetGameType() == VirtuaStriker4)
  {
    m_ic_card_data[0x22] = 0x44;
    m_ic_card_data[0x23] = 0x00;
  }

  // Use count
  m_ic_card_data[0x28] = 0xFF;
  m_ic_card_data[0x29] = 0xFF;
}

constexpr u32 SI_XFER_LENGTH_MASK = 0x7f;

// Translate [0,1,2,...,126,127] to [128,1,2,...,126,127]
constexpr s32 ConvertSILengthField(u32 field)
{
  return ((field - 1) & SI_XFER_LENGTH_MASK) + 1;
}

void CSIDevice_AMBaseboard::ICCardSendReply(ICCommand* iccommand, u8* buffer, u32* length)
{
  iccommand->status = Common::swap16(iccommand->status);

  const u8 crc = CheckSumXOR(iccommand->data + 2, iccommand->pktlen - 1);

  for (u32 i = 0; i < iccommand->pktlen + 1; ++i)
  {
    buffer[(*length)++] = iccommand->data[i];
  }

  buffer[(*length)++] = crc;
}

void CSIDevice_AMBaseboard::SwapBuffers(u8* buffer, u32* buffer_length)
{
  memcpy(m_last[1], buffer, 0x80);     // Save current buffer
  memcpy(buffer, m_last[0], 0x80);     // Load previous buffer
  memcpy(m_last[0], m_last[1], 0x80);  // Update history

  m_lastptr[1] = *buffer_length;  // Swap lengths
  *buffer_length = m_lastptr[0];
  m_lastptr[0] = m_lastptr[1];
}

int CSIDevice_AMBaseboard::RunBuffer(u8* buffer, int request_length)
{
  const auto& serial_interface = m_system.GetSerialInterface();
  u32 buffer_length = ConvertSILengthField(serial_interface.GetInLength());

  // Debug logging
  ISIDevice::RunBuffer(buffer, buffer_length);

  u32 buffer_position = 0;
  while (buffer_position < buffer_length)
  {
    BaseBoardCommand command = static_cast<BaseBoardCommand>(buffer[buffer_position]);
    buffer_position++;

    switch (command)
    {
    case BaseBoardCommand::GCAM_Reset:  // Returns ID and dip switches
    {
      const u32 id = Common::swap32(SI_AM_BASEBOARD | 0x100);
      std::memcpy(buffer, &id, sizeof(id));
      return sizeof(id);
    }
    break;
    case BaseBoardCommand::GCAM_Command:
    {
      u32 checksum = 0;
      for (u32 i = 0; i < buffer_length; ++i)
        checksum += buffer[i];

      std::array<u8, 0x80> data_out{};
      u32 data_offset = 0;

      static u32 dip_switch_1 = 0xFE;
      static u32 dip_switch_0 = 0xFF;

      data_out[data_offset++] = 1;
      data_out[data_offset++] = 1;

      u8* data_in = buffer + 2;
      const u8* data_in_end = buffer + buffer[buffer_position] + 2;

      while (data_in < data_in_end)
      {
        const u32 gcam_command = *data_in++;
        switch (GCAMCommand(gcam_command))
        {
        case GCAMCommand::StatusSwitches:
        {
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
          if (AMMediaboard::GetGameType() == FZeroAX ||
              AMMediaboard::GetGameType() == FZeroAXMonster)
          {
            dip_switch_0 &= ~0x20;
          }

          // Disable camera in MKGP1/2
          if (AMMediaboard::GetGameType() == MarioKartGP ||
              AMMediaboard::GetGameType() == MarioKartGP2)
          {
            dip_switch_0 &= ~0x10;
          }

          data_out[data_offset++] = dip_switch_0;
          data_out[data_offset++] = dip_switch_1;
          break;
        }
        case GCAMCommand::SerialNumber:
        {
          NOTICE_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x11, {:02x} (READ SERIAL NR)",
                         *data_in++);

          data_out[data_offset++] = gcam_command;
          data_out[data_offset++] = 16;
          memcpy(data_out.data() + data_offset, "AADE-01B98394904", 16);

          data_offset += 16;
          break;
        }
        case GCAMCommand::Unknown_12:
          NOTICE_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x12, {:02x} {:02x}", data_in[0],
                         data_in[1]);

          data_out[data_offset++] = gcam_command;
          data_out[data_offset++] = 0x00;

          data_in += 2;
          break;
        case GCAMCommand::Unknown_14:
          NOTICE_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x14, {:02x} {:02x}", data_in[0],
                         data_in[1]);

          data_out[data_offset++] = gcam_command;
          data_out[data_offset++] = 0x00;

          data_in += 2;
          break;
        case GCAMCommand::FirmVersion:
          NOTICE_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x15, {:02x} (READ FIRM VERSION)",
                         *data_in++);

          data_out[data_offset++] = gcam_command;
          data_out[data_offset++] = 0x02;
          // 00.26
          data_out[data_offset++] = 0x00;
          data_out[data_offset++] = 0x26;
          break;
        case GCAMCommand::FPGAVersion:
          NOTICE_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x16, {:02x} (READ FPGA VERSION)",
                         *data_in++);

          data_out[data_offset++] = gcam_command;
          data_out[data_offset++] = 0x02;
          // 07.06
          data_out[data_offset++] = 0x07;
          data_out[data_offset++] = 0x06;
          break;
        case GCAMCommand::RegionSettings:
        {
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
          DEBUG_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x21, {:02x}, {:02x}, {:02x}, {:02x}",
                        data_in[0], data_in[1], data_in[2], data_in[3]);
          data_in += 4;
        }
        break;
        // No reply
        // Note: Always sends six bytes
        case GCAMCommand::Unknown_22:
        {
          DEBUG_LOG_FMT(
              SERIALINTERFACE_AMBB,
              "GC-AM: Command 0x22, {:02x}, {:02x}, {:02x}, {:02x}, {:02x}, {:02x}, {:02x}",
              data_in[0], data_in[1], data_in[2], data_in[3], data_in[4], data_in[5], data_in[6]);
          data_in += data_in[0] + 1;
        }
        break;
        case GCAMCommand::Unknown_23:
          DEBUG_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x23, {:02x} {:02x}", data_in[0],
                        data_in[1]);

          data_out[data_offset++] = gcam_command;
          data_out[data_offset++] = 0x00;

          data_in += 2;
          break;
        case GCAMCommand::Unknown_24:
          DEBUG_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x24, {:02x} {:02x}", data_in[0],
                        data_in[1]);

          data_out[data_offset++] = gcam_command;
          data_out[data_offset++] = 0x00;

          data_in += 2;
          break;
        case GCAMCommand::SerialA:
        {
          u32 length = *data_in++;
          if (length)
          {
            INFO_LOG_FMT(SERIALINTERFACE_AMBB,
                         "GC-AM: Command 0x31, {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} "
                         "{:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x}",
                         length, data_in[0], data_in[1], data_in[2], data_in[3], data_in[4],
                         data_in[5], data_in[6], data_in[7], data_in[8], data_in[9], data_in[10],
                         data_in[11], data_in[12]);

            // Serial - Wheel
            if (AMMediaboard::GetGameType() == MarioKartGP ||
                AMMediaboard::GetGameType() == MarioKartGP2)
            {
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
                data_out[data_offset++] = 'E';  // Error
                data_out[data_offset++] = '0';
                data_out[data_offset++] = '0';
                m_wheel_init++;
                break;
              case 1:
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

              data_in += length;
              break;
            }

            // Serial - Unknown
            if (AMMediaboard::GetGameType() == GekitouProYakyuu)
            {
              const u32 serial_command = Common::BitCastPtr<u32>(data_in);

              if (serial_command == 0x00001000)
              {
                data_out[data_offset++] = gcam_command;
                data_out[data_offset++] = 0x03;
                data_out[data_offset++] = 1;
                data_out[data_offset++] = 2;
                data_out[data_offset++] = 3;
              }

              data_in += length;
              break;
            }

            // Serial IC-CARD / Serial Deck Reader
            if (AMMediaboard::GetGameType() == VirtuaStriker4 ||
                AMMediaboard::GetGameType() == VirtuaStriker4_2006 ||
                AMMediaboard::GetGameType() == KeyOfAvalon)
            {
              u32 serial_command = data_in[1];

              ICCommand icco;

              // Set default reply
              icco.pktcmd = gcam_command;
              icco.pktlen = 7;
              icco.fixed = 0x10;
              icco.command = serial_command;
              icco.flag = 0;
              icco.length = 2;
              icco.status = 0;
              icco.extlen = 0;

              // Check for rest of data from the write pages command
              if (m_ic_write_size && m_ic_write_offset)
              {
                const u32 size = data_in[1];

                DEBUG_LOG_FMT(SERIALINTERFACE_CARD, "Command: {}", HexDump(data_in, size + 2));

                INFO_LOG_FMT(
                    SERIALINTERFACE_CARD,
                    "GC-AM: Command 25 (IC-CARD) Write Pages: Off:{:x} Size:{:x} PSize:{:x}",
                    m_ic_write_offset, m_ic_write_size, size);

                memcpy(m_ic_write_buffer + m_ic_write_offset, data_in + 2, size);

                m_ic_write_offset += size;

                if (m_ic_write_offset > m_ic_write_size)
                {
                  m_ic_write_offset = 0;

                  const u16 page = m_ic_write_buffer[5];
                  const u16 count = m_ic_write_buffer[7];

                  memcpy(m_ic_card_data + page * 8, m_ic_write_buffer + 10, count * 8);

                  INFO_LOG_FMT(SERIALINTERFACE_CARD,
                               "GC-AM: Command 25 (IC-CARD) Write Pages:{} Count:{}({:x})", page,
                               count, size);

                  icco.command = WritePages;

                  ICCardSendReply(&icco, data_out.data(), &data_offset);
                }
                data_in += length;
                break;
              }

              switch (ICCARDCommand(serial_command))
              {
              case ICCARDCommand::GetStatus:
                icco.status = m_ic_card_state;

                INFO_LOG_FMT(SERIALINTERFACE_CARD,
                             "GC-AM: Command 0x31 (IC-CARD) Get Status:{:02x}", m_ic_card_state);
                break;
              case ICCARDCommand::SetBaudrate:
                INFO_LOG_FMT(SERIALINTERFACE_CARD, "GC-AM: Command 0x31 (IC-CARD) Set Baudrate");
                break;
              case ICCARDCommand::FieldOn:
                m_ic_card_state |= 0x10;
                INFO_LOG_FMT(SERIALINTERFACE_CARD, "GC-AM: Command 0x31 (IC-CARD) Field On");
                break;
              case ICCARDCommand::InsertCheck:
                icco.status = m_ic_card_status;
                INFO_LOG_FMT(SERIALINTERFACE_CARD,
                             "GC-AM: Command 0x31 (IC-CARD) Insert Check:{:02x}", m_ic_card_status);
                break;
              case ICCARDCommand::AntiCollision:
                icco.extlen = 8;
                icco.length += icco.extlen;
                icco.pktlen += icco.extlen;

                // Card ID
                icco.extdata[0] = 0x00;
                icco.extdata[1] = 0x00;
                icco.extdata[2] = 0x54;
                icco.extdata[3] = 0x4D;
                icco.extdata[4] = 0x50;
                icco.extdata[5] = 0x00;
                icco.extdata[6] = 0x00;
                icco.extdata[7] = 0x00;

                INFO_LOG_FMT(SERIALINTERFACE_CARD, "GC-AM: Command 0x31 (IC-CARD) Anti Collision");
                break;
              case ICCARDCommand::SelectCard:
                icco.extlen = 8;
                icco.length += icco.extlen;
                icco.pktlen += icco.extlen;

                // Session
                icco.extdata[0] = 0x00;
                icco.extdata[1] = m_ic_card_session;
                icco.extdata[2] = 0x00;
                icco.extdata[3] = 0x00;
                icco.extdata[4] = 0x00;
                icco.extdata[5] = 0x00;
                icco.extdata[6] = 0x00;
                icco.extdata[7] = 0x00;

                INFO_LOG_FMT(SERIALINTERFACE_CARD, "GC-AM: Command 0x31 (IC-CARD) Select Card:{}",
                             m_ic_card_session);
                break;
              case ICCARDCommand::ReadPage:
              case ICCARDCommand::ReadUseCount:
              {
                const u16 page = Common::swap16(data_in + 6) & 0xFF;  // 255 is max page

                icco.extlen = 8;
                icco.length += icco.extlen;
                icco.pktlen += icco.extlen;

                memcpy(icco.extdata, m_ic_card_data + page * 8, 8);

                INFO_LOG_FMT(SERIALINTERFACE_CARD, "GC-AM: Command 31 (IC-CARD) Read Page:{}",
                             page);
                break;
              }
              case ICCARDCommand::WritePage:
              {
                const u16 page = Common::swap16(data_in + 8) & 0xFF;  // 255 is max page

                // Write only one page
                if (page == 4)
                {
                  icco.status = 0x80;
                }
                else
                {
                  memcpy(m_ic_card_data + page * 8, data_in + 10, 8);
                }

                INFO_LOG_FMT(SERIALINTERFACE_CARD, "GC-AM: Command 0x31 (IC-CARD) Write Page:{}",
                             page);
                break;
              }
              case ICCARDCommand::DecreaseUseCount:
              {
                const u16 page = Common::swap16(data_in + 6) & 0xFF;  // 255 is max page

                icco.extlen = 2;
                icco.length += icco.extlen;
                icco.pktlen += icco.extlen;

                auto ic_card_data = Common::BitCastPtr<u16>(m_ic_card_data + 0x28);
                ic_card_data = ic_card_data - 1;

                // Counter
                icco.extdata[0] = m_ic_card_data[0x28];
                icco.extdata[1] = m_ic_card_data[0x29];

                INFO_LOG_FMT(SERIALINTERFACE_CARD,
                             "GC-AM: Command 31 (IC-CARD) Decrease Use Count:{}", page);
                break;
              }
              case ICCARDCommand::ReadPages:
              {
                const u16 page = Common::swap16(data_in + 6) & 0xFF;  // 255 is max page
                const u16 count = Common::swap16(data_in + 8);

                const u32 offs = page * 8;
                u32 cnt = count * 8;

                // Limit read size to not overwrite the reply buffer
                if (cnt > (u32)0x50 - data_offset)
                {
                  cnt = 5 * 8;
                }

                icco.extlen = cnt;
                icco.length += icco.extlen;
                icco.pktlen += icco.extlen;

                memcpy(icco.extdata, m_ic_card_data + offs, cnt);

                INFO_LOG_FMT(SERIALINTERFACE_CARD,
                             "GC-AM: Command 31 (IC-CARD) Read Pages:{} Count:{}", page, count);
                break;
              }
              case ICCARDCommand::WritePages:
              {
                const u16 pksize = length;
                const u16 size = Common::swap16(data_in + 2);
                const u16 page = Common::swap16(data_in + 6) & 0xFF;  // 255 is max page
                const u16 count = Common::swap16(data_in + 8);

                // We got a complete packet
                if (pksize - 5 == size)
                {
                  if (page == 4)  // Read Only Page, must return error
                  {
                    icco.status = 0x80;
                  }
                  else
                  {
                    if (page * 8 + count * 8 > sizeof(m_ic_card_data))
                    {
                      ERROR_LOG_FMT(
                          SERIALINTERFACE_CARD,
                          "GC-AM: Command 0x31 (IC-CARD) Data overflow: Pages:{} Count:{}({:x})",
                          page, count, size);
                    }
                    else
                    {
                      memcpy(m_ic_card_data + page * 8, data_in + 13, count * 8);
                    }
                  }

                  INFO_LOG_FMT(SERIALINTERFACE_CARD,
                               "GC-AM: Command 0x31 (IC-CARD) Write Pages:{} Count:{}({:x})", page,
                               count, size);
                }
                // VirtuaStriker 4 splits the writes over multiple packets
                else
                {
                  memcpy(m_ic_write_buffer, data_in + 2, pksize);
                  m_ic_write_offset += pksize;
                  m_ic_write_size = size;
                }
                break;
              }
              default:
                // Handle Deck Reader commands
                serial_command = data_in[0];
                icco.command = serial_command;
                icco.flag = 0;
                switch (CDReaderCommand(serial_command))
                {
                case CDReaderCommand::ProgramVersion:
                  INFO_LOG_FMT(SERIALINTERFACE_CARD,
                               "GC-AM: Command 0x31 (DECK READER) Program Version");

                  icco.extlen = (u32)strlen(s_cdr_program_version);
                  icco.length += icco.extlen;
                  icco.pktlen += icco.extlen;

                  memcpy(icco.extdata, s_cdr_program_version, icco.extlen);
                  break;
                case CDReaderCommand::BootVersion:
                  INFO_LOG_FMT(SERIALINTERFACE_CARD,
                               "GC-AM: Command 0x31 (DECK READER) Boot Version");

                  icco.extlen = (u32)strlen(s_cdr_boot_version);
                  icco.length += icco.extlen;
                  icco.pktlen += icco.extlen;

                  memcpy(icco.extdata, s_cdr_boot_version, icco.extlen);
                  break;
                case CDReaderCommand::ShutterGet:
                  INFO_LOG_FMT(SERIALINTERFACE_CARD,
                               "GC-AM: Command 0x31 (DECK READER) Shutter Get");

                  icco.extlen = 4;
                  icco.length += icco.extlen;
                  icco.pktlen += icco.extlen;

                  icco.extdata[0] = 0;
                  icco.extdata[1] = 0;
                  icco.extdata[2] = 0;
                  icco.extdata[3] = 0;
                  break;
                case CDReaderCommand::CameraCheck:
                  INFO_LOG_FMT(SERIALINTERFACE_CARD,
                               "GC-AM: Command 0x31 (DECK READER) Camera Check");

                  icco.extlen = 6;
                  icco.length += icco.extlen;
                  icco.pktlen += icco.extlen;

                  icco.extdata[0] = 0x23;
                  icco.extdata[1] = 0x28;
                  icco.extdata[2] = 0x45;
                  icco.extdata[3] = 0x29;
                  icco.extdata[4] = 0x45;
                  icco.extdata[5] = 0x29;
                  break;
                case CDReaderCommand::ProgramChecksum:
                  INFO_LOG_FMT(SERIALINTERFACE_CARD,
                               "GC-AM: Command 0x31 (DECK READER) Program Checksum");

                  icco.extlen = 4;
                  icco.length += icco.extlen;
                  icco.pktlen += icco.extlen;

                  icco.extdata[0] = 0x23;
                  icco.extdata[1] = 0x28;
                  icco.extdata[2] = 0x45;
                  icco.extdata[3] = 0x29;
                  break;
                case CDReaderCommand::BootChecksum:
                  INFO_LOG_FMT(SERIALINTERFACE_CARD,
                               "GC-AM: Command 0x31 (DECK READER) Boot Checksum");

                  icco.extlen = 4;
                  icco.length += icco.extlen;
                  icco.pktlen += icco.extlen;

                  icco.extdata[0] = 0x23;
                  icco.extdata[1] = 0x28;
                  icco.extdata[2] = 0x45;
                  icco.extdata[3] = 0x29;
                  break;
                case CDReaderCommand::SelfTest:
                  INFO_LOG_FMT(SERIALINTERFACE_CARD, "GC-AM: Command 0x31 (DECK READER) Self Test");
                  icco.flag = 0x00;
                  break;
                case CDReaderCommand::SensLock:
                  INFO_LOG_FMT(SERIALINTERFACE_CARD, "GC-AM: Command 0x31 (DECK READER) Sens Lock");
                  icco.flag = 0x01;
                  break;
                case CDReaderCommand::SensCard:
                  INFO_LOG_FMT(SERIALINTERFACE_CARD, "GC-AM: Command 0x31 (DECK READER) Sens Card");
                  break;
                case CDReaderCommand::ShutterCard:
                  INFO_LOG_FMT(SERIALINTERFACE_CARD,
                               "GC-AM: Command 0x31 (DECK READER) Shutter Card");
                  break;
                case CDReaderCommand::ReadCard:
                  INFO_LOG_FMT(SERIALINTERFACE_CARD, "GC-AM: Command 0x31 (DECK READER) Read Card");

                  icco.fixed = 0xAA;
                  icco.flag = 0xAA;
                  icco.extlen = sizeof(s_cdr_card_data);
                  icco.length = 0x72;
                  icco.status = Common::swap16(icco.extlen);

                  icco.pktlen += icco.extlen;

                  memcpy(icco.extdata, s_cdr_card_data, sizeof(s_cdr_card_data));

                  break;
                default:
                  WARN_LOG_FMT(SERIALINTERFACE_CARD,
                               "GC-AM: Command 0x31 (IC-Card) {:02x} {:02x} {:02x} {:02x} {:02x} "
                               "{:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x}",
                               data_in[2], data_in[3], data_in[4], data_in[5], data_in[6],
                               data_in[7], data_in[8], data_in[9], data_in[10], data_in[11],
                               data_in[12], data_in[13]);
                  break;
                }
                break;
              }

              ICCardSendReply(&icco, data_out.data(), &data_offset);

              data_in += length;
              break;
            }
          }

          u32 command_offset = 0;
          while (command_offset < length)
          {
            // All commands are OR'd with 0x80
            // Last byte is checksum which we don't care about
            const u32 serial_command = Common::swap32(data_in + command_offset) ^ 0x80000000;

            if (AMMediaboard::GetGameType() == FZeroAX ||
                AMMediaboard::GetGameType() == FZeroAXMonster)
            {
              INFO_LOG_FMT(SERIALINTERFACE_AMBB,
                           "GC-AM: Command 0x31 (MOTOR) Length:{:02x} Command:{:04x}({:02x})",
                           length, (serial_command >> 8) & 0xFFFF, serial_command >> 24);
            }
            else
            {
              INFO_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x31 (SERIAL) Command:{:06x}",
                           serial_command);

              if (serial_command == 0x801000)
              {
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
                                   "GC-AM: Command 0x31 (MOTOR) mapped_strength:{}",
                                   mapped_strength);
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

              memcpy(data_out.data() + data_offset, m_motor_reply, m_motor_reply[1] + 2);
              data_offset += m_motor_reply[1] + 2;
            }
          }

          data_in += length;
          break;
        }
        case GCAMCommand::SerialB:
        {
          DEBUG_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 32 (CARD-Interface)");
          const u32 length = *data_in++;
          if (length)
          {
            // Send Card Reply
            if (length == 1 && data_in[0] == 0x05)
            {
              if (m_card_read_length)
              {
                data_out[data_offset++] = gcam_command;
                u32 read_length = m_card_read_length - m_card_read;

                if (AMMediaboard::GetGameType() == FZeroAX)
                {
                  read_length = std::min<u32>(read_length, 0x2F);
                }

                data_out[data_offset++] = read_length;  // 0x2F (max size per packet)

                memcpy(data_out.data() + data_offset, m_card_read_packet + m_card_read,
                       read_length);

                data_offset += read_length;
                m_card_read += read_length;

                if (m_card_read >= m_card_read_length)
                  m_card_read_length = 0;

                data_in += length;
                break;
              }

              data_out[data_offset++] = gcam_command;
              const u32 command_length_offset = data_offset;
              data_out[data_offset++] = 0x00;  // len

              data_out[data_offset++] = 0x02;
              const u32 checksum_start = data_offset;

              data_out[data_offset++] = 0x00;  // 0x00 len

              data_out[data_offset++] = m_card_command;  // 0x01 command

              switch (CARDCommand(m_card_command))
              {
              case CARDCommand::Init:
                data_out[data_offset++] = 0x00;  // 0x02
                data_out[data_offset++] = 0x30;  // 0x03
                break;
              case CARDCommand::GetState:
                data_out[data_offset++] = 0x20 | m_card_bit;  // 0x02

                // bit 0: Please take your card
                // bit 1: Endless waiting causes UNK_E to be called
                data_out[data_offset++] = 0x00;  // 0x03
                break;
              case CARDCommand::Read:
                data_out[data_offset++] = 0x02;  // 0x02
                data_out[data_offset++] = 0x53;  // 0x03
                break;
              case CARDCommand::IsPresent:
                data_out[data_offset++] = 0x22;  // 0x02
                data_out[data_offset++] = 0x30;  // 0x03
                break;
              case CARDCommand::Write:
                data_out[data_offset++] = 0x02;  // 0x02
                data_out[data_offset++] = 0x00;  // 0x03
                break;
              case CARDCommand::SetPrintParam:
              case CARDCommand::RegisterFont:
                data_out[data_offset++] = 0x00;  // 0x02
                data_out[data_offset++] = 0x00;  // 0x03
                break;
              case CARDCommand::WriteInfo:
                data_out[data_offset++] = 0x02;  // 0x02
                data_out[data_offset++] = 0x00;  // 0x03
                break;
                // TODO: CARDCommand::Erase is not handled.
                ERROR_LOG_FMT(SERIALINTERFACE_AMBB, "CARDCommand::Erase is not handled.");
                break;
              case CARDCommand::Eject:
                if (AMMediaboard::GetGameType() == FZeroAX)
                {
                  data_out[data_offset++] = 0x01;  // 0x02
                }
                else
                {
                  data_out[data_offset++] = 0x31;  // 0x02
                }
                data_out[data_offset++] = 0x30;  // 0x03
                break;
              case CARDCommand::Clean:
                data_out[data_offset++] = 0x02;  // 0x02
                data_out[data_offset++] = 0x00;  // 0x03
                break;
              case CARDCommand::Load:
                data_out[data_offset++] = 0x02;  // 0x02
                data_out[data_offset++] = 0x30;  // 0x03
                break;
              case CARDCommand::SetShutter:
                data_out[data_offset++] = 0x00;  // 0x02
                data_out[data_offset++] = 0x00;  // 0x03
                break;
              }

              data_out[data_offset++] = 0x30;  // 0x04
              data_out[data_offset++] = 0x00;  // 0x05

              data_out[data_offset++] = 0x03;  // 0x06

              data_out[checksum_start] = data_offset - checksum_start;  // 0x00 len

              data_out[data_offset] = 0;  // 0x07
              for (u32 i = 0; i < data_out[checksum_start]; ++i)
                data_out[data_offset] ^= data_out[checksum_start + i];

              data_offset++;

              data_out[command_length_offset] = data_out[checksum_start] + 2;
            }
            else
            {
              for (u32 i = 0; i < length; ++i)
                m_card_buffer[m_card_offset + i] = data_in[i];

              m_card_offset += length;

              // Check if we got a complete command
              if (m_card_buffer[0] == 0x02)
              {
                if (m_card_buffer[1] == m_card_offset - 2)
                {
                  if (m_card_buffer[m_card_offset - 2] == 0x03)
                  {
                    m_card_command = m_card_buffer[2];

                    switch (CARDCommand(m_card_command))
                    {
                    case CARDCommand::Init:
                      NOTICE_LOG_FMT(SERIALINTERFACE_CARD, "GC-AM: Command CARD Init");

                      m_card_write_length = 0;
                      m_card_bit = 0;
                      m_card_memory_size = 0;
                      m_card_state_call_count = 0;
                      break;
                    case CARDCommand::GetState:
                    {
                      NOTICE_LOG_FMT(SERIALINTERFACE_CARD, "GC-AM: Command CARD GetState({:02X})",
                                     m_card_bit);

                      if (m_card_memory_size == 0)
                      {
                        const std::string card_filename(
                            fmt::format("{}tricard_{}.bin", File::GetUserPath(D_TRIUSER_IDX),
                                        SConfig::GetInstance().GetGameID()));

                        if (File::Exists(card_filename))
                        {
                          File::IOFile card(card_filename, "rb+");
                          m_card_memory_size = (u32)card.GetSize();

                          card.ReadBytes(m_card_memory, m_card_memory_size);
                          card.Close();

                          m_card_is_inserted = true;
                        }
                      }

                      if (AMMediaboard::GetGameType() == FZeroAX && m_card_memory_size)
                      {
                        m_card_state_call_count++;
                        if (m_card_state_call_count > 10)
                        {
                          if (m_card_bit & 2)
                            m_card_bit &= ~2u;
                          else
                            m_card_bit |= 2;

                          m_card_state_call_count = 0;
                        }
                      }

                      if (m_card_clean == 1)
                      {
                        m_card_clean = 2;
                      }
                      else if (m_card_clean == 2)
                      {
                        const std::string card_filename(
                            fmt::format("{}tricard_{}.bin", File::GetUserPath(D_TRIUSER_IDX),
                                        SConfig::GetInstance().GetGameID()));

                        if (File::Exists(card_filename))
                        {
                          m_card_memory_size = (u32)File::GetSize(card_filename);
                          if (m_card_memory_size)
                          {
                            if (AMMediaboard::GetGameType() == FZeroAX)
                            {
                              m_card_bit = 2;
                            }
                            else
                            {
                              m_card_bit = 1;
                            }
                          }
                        }
                        m_card_clean = 0;
                      }
                      break;
                    }
                    case CARDCommand::IsPresent:
                      NOTICE_LOG_FMT(SERIALINTERFACE_CARD, "GC-AM: Command CARD IsPresent");
                      break;
                    case CARDCommand::RegisterFont:
                      NOTICE_LOG_FMT(SERIALINTERFACE_CARD, "GC-AM: Command CARD RegisterFont");
                      break;
                    case CARDCommand::Load:
                    {
                      u8 mode = m_card_buffer[6];
                      NOTICE_LOG_FMT(SERIALINTERFACE_CARD, "GC-AM: Command CARD Load({:02X})",
                                     mode);
                      break;
                    }
                    case CARDCommand::Clean:
                      NOTICE_LOG_FMT(SERIALINTERFACE_CARD, "GC-AM: Command CARD Clean");
                      m_card_clean = 1;
                      break;
                    case CARDCommand::Read:
                    {
                      const u8 mode = m_card_buffer[6];
                      const u8 bitmode = m_card_buffer[7];
                      const u8 track = m_card_buffer[8];

                      NOTICE_LOG_FMT(SERIALINTERFACE_CARD,
                                     "GC-AM: Command CARD Read({:02X},{:02X},{:02X})", mode,
                                     bitmode, track);

                      // Prepare read packet
                      memset(m_card_read_packet, 0, 0xDB);
                      u32 packet_offset = 0;

                      const std::string card_filename(
                          fmt::format("{}tricard_{}.bin", File::GetUserPath(D_TRIUSER_IDX),
                                      SConfig::GetInstance().GetGameID()));

                      if (File::Exists(card_filename))
                      {
                        File::IOFile card(card_filename, "rb+");
                        if (m_card_memory_size == 0)
                        {
                          m_card_memory_size = (u32)card.GetSize();
                        }

                        card.ReadBytes(m_card_memory, m_card_memory_size);
                        card.Close();

                        m_card_is_inserted = true;
                      }

                      m_card_read_packet[packet_offset++] = 0x02;  // SUB CMD
                      m_card_read_packet[packet_offset++] = 0x00;  // SUB CMDLen

                      m_card_read_packet[packet_offset++] = 0x33;  // CARD CMD

                      if (m_card_is_inserted)  // CARD Status
                      {
                        m_card_read_packet[packet_offset++] = 0x31;
                      }
                      else
                      {
                        m_card_read_packet[packet_offset++] = 0x30;
                      }

                      m_card_read_packet[packet_offset++] = 0x30;
                      m_card_read_packet[packet_offset++] = 0x30;

                      // Data reply
                      memcpy(m_card_read_packet + packet_offset, m_card_memory, m_card_memory_size);
                      packet_offset += m_card_memory_size;

                      m_card_read_packet[packet_offset++] = 0x03;

                      m_card_read_packet[1] = packet_offset - 1;  // SUB CMDLen

                      for (u32 i = 0; i < packet_offset - 1; ++i)
                        m_card_read_packet[packet_offset] ^= m_card_read_packet[1 + i];

                      packet_offset++;

                      m_card_read_length = packet_offset;
                      m_card_read = 0;
                      break;
                    }
                    case CARDCommand::Write:
                    {
                      const u8 mode = m_card_buffer[6];
                      const u8 bitmode = m_card_buffer[7];
                      const u8 track = m_card_buffer[8];

                      m_card_memory_size = m_card_buffer[1] - 9;

                      memcpy(m_card_memory, m_card_buffer + 9, m_card_memory_size);

                      NOTICE_LOG_FMT(SERIALINTERFACE_CARD,
                                     "GC-AM: Command CARD Write: {:02X} {:02X} {:02X} {}", mode,
                                     bitmode, track, m_card_memory_size);

                      const std::string card_filename(File::GetUserPath(D_TRIUSER_IDX) +
                                                      "tricard_" +
                                                      SConfig::GetInstance().GetGameID() + ".bin");

                      File::IOFile card(card_filename, "wb+");
                      card.WriteBytes(m_card_memory, m_card_memory_size);
                      card.Close();

                      m_card_bit = 2;

                      m_card_state_call_count = 0;
                      break;
                    }
                    case CARDCommand::SetPrintParam:
                      NOTICE_LOG_FMT(SERIALINTERFACE_CARD, "GC-AM: Command CARD SetPrintParam");
                      break;
                    case CARDCommand::WriteInfo:
                      NOTICE_LOG_FMT(SERIALINTERFACE_CARD, "GC-AM: Command CARD WriteInfo");
                      break;
                    case CARDCommand::Erase:
                      NOTICE_LOG_FMT(SERIALINTERFACE_CARD, "GC-AM: Command CARD Erase");
                      break;
                    case CARDCommand::Eject:
                      NOTICE_LOG_FMT(SERIALINTERFACE_CARD, "GC-AM: Command CARD Eject");
                      if (AMMediaboard::GetGameType() != FZeroAX)
                      {
                        m_card_bit = 0;
                      }
                      break;
                    case CARDCommand::SetShutter:
                      NOTICE_LOG_FMT(SERIALINTERFACE_CARD, "GC-AM: Command CARD SetShutter");
                      if (AMMediaboard::GetGameType() != FZeroAX)
                      {
                        m_card_bit = 0;
                      }
                      // Close
                      if (m_card_buffer[6] == 0x30)
                      {
                        m_card_shutter = false;
                      }
                      // Open
                      else if (m_card_buffer[6] == 0x31)
                      {
                        m_card_shutter = true;
                      }
                      break;
                    default:
                      ERROR_LOG_FMT(SERIALINTERFACE_CARD, "GC-AM: CARD:Unhandled command!");
                      ERROR_LOG_FMT(SERIALINTERFACE_CARD, "GC-AM: CARD:[{:08X}]", m_card_command);
                      // hexdump( m_card_buffer, m_card_offset );
                      break;
                    }
                    m_card_offset = 0;
                  }
                }
              }

              data_out[data_offset++] = 0x32;
              data_out[data_offset++] = 0x01;  // len
              data_out[data_offset++] = 0x06;  // OK
            }
          }
          else
          {
            data_out[data_offset++] = gcam_command;
            data_out[data_offset++] = 0x00;  // len
          }
          data_in += length;
          break;
        }
        case GCAMCommand::JVSIOA:
        case GCAMCommand::JVSIOB:
        {
          DEBUG_LOG_FMT(
              SERIALINTERFACE_JVSIO,
              "GC-AM: Command {:02x}, {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} (JVS IO)",
              gcam_command, data_in[0], data_in[1], data_in[2], data_in[3], data_in[4], data_in[5],
              data_in[6]);

          JVSIOMessage message;

          static int delay = 0;

          const u8* frame = &data_in[0];
          const u8 nr_bytes = frame[3];  // Byte after E0 xx
          u32 frame_len = nr_bytes + 3;  // Header(2) + length byte + payload + checksum

          u8 jvs_buf[0x80];

          frame_len = std::min<u32>(frame_len, sizeof(jvs_buf));

          memcpy(jvs_buf, frame, frame_len);

          // Extract node and payload pointers
          u8 node = jvs_buf[2];
          u8* jvs_io = jvs_buf + 4;                 // First payload byte
          const u8* jvs_end = jvs_buf + frame_len;  // One byte before checksum

          message.Start(0);
          message.AddData(1);

          // Now iterate over the payload
          while (jvs_io < jvs_end)
          {
            int jvsio_command = *jvs_io++;
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
                // 2 Player (13bit), 2 Coin slot, 4 Analog-in, 1 CARD, 8 Driver-out
                message.AddData("\x01\x02\x0D\x00", 4);
                message.AddData("\x02\x02\x00\x00", 4);
                message.AddData("\x10\x01\x00\x00", 4);
                message.AddData("\x12\x08\x00\x00", 4);
                message.AddData("\x00\x00\x00\x00", 4);
                break;
              case GekitouProYakyuu:
                // 2 Player (13bit), 2 Coin slot, 4 Analog-in, 1 CARD, 8 Driver-out
                message.AddData("\x01\x02\x0D\x00", 4);
                message.AddData("\x02\x02\x00\x00", 4);
                message.AddData("\x03\x04\x00\x00", 4);
                message.AddData("\x10\x01\x00\x00", 4);
                message.AddData("\x12\x08\x00\x00", 4);
                message.AddData("\x00\x00\x00\x00", 4);
                break;
              case VirtuaStriker4:
              case VirtuaStriker4_2006:
                // 2 Player (13bit), 1 Coin slot, 4 Analog-in, 1 CARD
                message.AddData("\x01\x02\x0D\x00", 4);
                message.AddData("\x02\x01\x00\x00", 4);
                message.AddData("\x03\x04\x00\x00", 4);
                message.AddData("\x10\x01\x00\x00", 4);
                message.AddData("\x00\x00\x00\x00", 4);
                break;
              case KeyOfAvalon:
                // 1 Player (15bit), 1 Coin slot, 3 Analog-in, Touch, 1 CARD, 1 Driver-out
                // (Unconfirmed)
                message.AddData("\x01\x01\x0F\x00", 4);
                message.AddData("\x02\x01\x00\x00", 4);
                message.AddData("\x03\x03\x00\x00", 4);
                message.AddData("\x06\x10\x10\x01", 4);
                message.AddData("\x10\x01\x00\x00", 4);
                message.AddData("\x12\x01\x00\x00", 4);
                message.AddData("\x00\x00\x00\x00", 4);
                break;
              case MarioKartGP:
              case MarioKartGP2:
              default:
                // 1 Player (15bit), 1 Coin slot, 3 Analog-in, 1 CARD, 1 Driver-out
                message.AddData("\x01\x01\x0F\x00", 4);
                message.AddData("\x02\x01\x00\x00", 4);
                message.AddData("\x03\x03\x00\x00", 4);
                message.AddData("\x10\x01\x00\x00", 4);
                message.AddData("\x12\x01\x00\x00", 4);
                message.AddData("\x00\x00\x00\x00", 4);
                break;
              }
              NOTICE_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Command 0x14, CheckFunctionality");
              break;
            case JVSIOCommand::MainID:
              while (*jvs_io++)
              {
              }
              message.AddData(StatusOkay);
              break;
            case JVSIOCommand::SwitchesInput:
            {
              u32 player_count = *jvs_io++;
              u32 player_byte_count = *jvs_io++;

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
                u8 player_data[3]{};

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

                  if (i == 0)
                  {
                    player_data[0] |= 0x10;  // IC-Card Switch ON

                    // IC-Card Lock
                    if (pad_status.button & PAD_BUTTON_DOWN)
                      player_data[1] |= 0x20;
                  }
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
                  // Toggle inserted card
                  if (pad_status.button & PAD_TRIGGER_L)
                  {
                    m_ic_card_status ^= ICCARDStatus::NoCard;
                  }
                }
                break;
                }

                for (u32 j = 0; j < player_byte_count; ++j)
                  message.AddData(player_data[j]);
              }
              break;
            }
            case JVSIOCommand::CoinInput:
            {
              const u32 slots = *jvs_io++;
              message.AddData(StatusOkay);
              for (u32 i = 0; i < slots; i++)
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
              message.AddData(StatusOkay);

              const u32 analogs = *jvs_io++;
              GCPadStatus pad_status = Pad::GetStatus(0);

              DEBUG_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Command 0x22, AnalogInput: {}",
                            analogs);

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

              DEBUG_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Command 0x25, PositionInput:{}",
                            channel);
              break;
            }
            case JVSIOCommand::CoinSubOutput:
            {
              const u32 slot = *jvs_io++;
              const u8 coinh = *jvs_io++;
              const u8 coinl = *jvs_io++;

              m_coin[slot] -= (coinh << 8) | coinl;

              message.AddData(StatusOkay);
              DEBUG_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Command 0x30, CoinSubOutput: {}", slot);
              break;
            }
            case JVSIOCommand::GeneralDriverOutput:
            {
              const u32 bytes = *jvs_io++;

              if (bytes)
              {
                message.AddData(StatusOkay);

                // The lamps are controlled via this
                if (AMMediaboard::GetGameType() == MarioKartGP)
                {
                  const u32 status = *jvs_io++;
                  if (status & 4)
                  {
                    DEBUG_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Command 32, Item Button ON");
                  }
                  else
                  {
                    DEBUG_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Command 32, Item Button OFF");
                  }
                  if (status & 8)
                  {
                    DEBUG_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Command 32, Cancel Button ON");
                  }
                  else
                  {
                    DEBUG_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Command 32, Cancel Button OFF");
                  }
                  break;
                }

                Common::UniqueBuffer<u8> buf(bytes);

                for (u32 i = 0; i < bytes; ++i)
                {
                  buf[i] = *jvs_io++;
                }

                INFO_LOG_FMT(
                    SERIALINTERFACE_JVSIO,
                    "JVS-IO: Command 0x32, GPO: {:02x} {:02x} {} {:02x}{:02x}{:02x} ({:02x})",
                    delay, m_rx_reply, bytes, buf[0], buf[1], buf[2],
                    Common::swap16(buf.data() + 1) >> 2);

                // Handling of the motion seat used in F-Zero AXs DX version
                switch (Common::swap16(buf.data() + 1) >> 2)
                {
                case 0x70:
                  delay++;
                  if ((delay % 10) == 0)
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
              break;
            }
            case JVSIOCommand::CoinAddOutput:
            {
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
              const u32 namco_command = *jvs_io++;

              if (namco_command == 0x18)
              {  // ID check
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
              if (*jvs_io++ == 0xD9)
              {
                NOTICE_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Command 0xF0, Reset");
                delay = 0;
                m_wheel_init = 0;
                m_ic_card_state = 0x20;
              }
              message.AddData(StatusOkay);

              dip_switch_1 |= 1;
              break;
            case JVSIOCommand::SetAddress:
              node = *jvs_io++;
              NOTICE_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Command 0xF1, SetAddress: node={}",
                             node);
              message.AddData(node == 1);
              dip_switch_1 &= ~1u;
              break;
            default:
              ERROR_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Unhandled: node={}, command={:02x}",
                            node, jvsio_command);
              break;
            }
          }

          message.End();

          data_out[data_offset++] = gcam_command;

          const u8* buf = message.m_message.data();
          const u32 len = message.m_pointer;
          data_out[data_offset++] = len;

          for (u32 i = 0; i < len; ++i)
            data_out[data_offset++] = buf[i];

          data_in += frame[0] + 1;
          break;
        }
        case GCAMCommand::Unknown_60:
          NOTICE_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x60, {:02x} {:02x} {:02x}",
                         data_in[0], data_in[1], data_in[2]);
          data_in += data_in[0] + 1;
          break;
        default:
          ERROR_LOG_FMT(SERIALINTERFACE_AMBB,
                        "GC-AM: Command {:02x} (unknown) {:02x} {:02x} {:02x} {:02x} {:02x}",
                        gcam_command, data_in[0], data_in[1], data_in[2], data_in[3], data_in[4]);
          break;
        }
      }

      memset(buffer, 0, buffer_length);

      data_in = buffer;
      data_out[1] = data_offset - 2;
      checksum = 0;

      for (int i = 0; i < 0x7F; ++i)
      {
        checksum += data_in[i] = data_out[i];
      }
      data_in[0x7f] = ~checksum;
      DEBUG_LOG_FMT(SERIALINTERFACE_AMBB, "Command send back: {}", HexDump(data_out.data(), 0x7F));

      SwapBuffers(buffer, &buffer_length);

      buffer_position = buffer_length;
      break;
    }
    default:
    {
      ERROR_LOG_FMT(SERIALINTERFACE, "Unknown SI command (0x{:08x})", (u32)command);
      PanicAlertFmt("SI: Unknown command");
      buffer_position = buffer_length;
    }
    break;
    }
  }

  return buffer_position;
}

u32 CSIDevice_AMBaseboard::MapPadStatus(const GCPadStatus& pad_status)
{
  // Thankfully changing mode does not change the high bits ;)
  u32 hi = 0;
  hi = pad_status.stickY;
  hi |= pad_status.stickX << 8;
  hi |= (pad_status.button | PAD_USE_ORIGIN) << 16;
  return hi;
}

CSIDevice_AMBaseboard::EButtonCombo
CSIDevice_AMBaseboard::HandleButtonCombos(const GCPadStatus& pad_status)
{
  // Keep track of the special button combos (embedded in controller hardware... :( )
  EButtonCombo temp_combo = COMBO_NONE;
  if ((pad_status.button & 0xff00) == (PAD_BUTTON_Y | PAD_BUTTON_X | PAD_BUTTON_START))
    temp_combo = COMBO_ORIGIN;
  else if ((pad_status.button & 0xff00) == (PAD_BUTTON_B | PAD_BUTTON_X | PAD_BUTTON_START))
    temp_combo = COMBO_RESET;

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

// GetData

// Return true on new data (max 7 Bytes and 6 bits ;)
// [00?SYXBA] [1LRZUDRL] [x] [y] [cx] [cy] [l] [r]
//  |\_ ERR_LATCH (error latched - check SISR)
//  |_ ERR_STATUS (error on last GetData or SendCmd?)
DataResponse CSIDevice_AMBaseboard::GetData(u32& hi, u32& low)
{
  GCPadStatus pad_status = GetPadStatus();

  if (!pad_status.isConnected)
    return DataResponse::ErrorNoResponse;

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

  return DataResponse::Success;
}

void CSIDevice_AMBaseboard::SendCommand(u32 command, u8 poll)
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

void CSIDevice_AMBaseboard::SetOrigin(const GCPadStatus& pad_status)
{
  m_origin.origin_stick_x = pad_status.stickX;
  m_origin.origin_stick_y = pad_status.stickY;
  m_origin.substick_x = pad_status.substickX;
  m_origin.substick_y = pad_status.substickY;
  m_origin.trigger_left = pad_status.triggerLeft;
  m_origin.trigger_right = pad_status.triggerRight;
}

void CSIDevice_AMBaseboard::HandleMoviePadStatus(Movie::MovieManager& movie, int device_number,
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

GCPadStatus CSIDevice_AMBaseboard::GetPadStatus()
{
  GCPadStatus pad_status = {};

  // For netplay, the local controllers are polled in GetNetPads(), and
  // the remote controllers receive their status there as well
  if (!NetPlay::IsNetPlayRunning())
  {
    pad_status = Pad::GetStatus(m_device_number);
  }

  // Our GCAdapter code sets PAD_GET_ORIGIN when a new device has been connected.
  // Watch for this to calibrate real controllers on connection.
  if (pad_status.button & PAD_GET_ORIGIN)
    SetOrigin(pad_status);

  return pad_status;
}

}  // namespace SerialInterface
