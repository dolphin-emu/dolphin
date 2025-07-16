// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// #pragma warning(disable : 4189)

#include "Core/HW/SI/SI_DeviceAMBaseboard.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <fmt/format.h>

#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Common/Hash.h"
#include "Common/IOFile.h"
#include "Common/IniFile.h"
#include "Common/Logging/Log.h"
#include "Core/Boot/Boot.h"
#include "Core/BootManager.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/ConfigLoaders/BaseConfigLoader.h"
#include "Core/ConfigLoaders/NetPlayConfigLoader.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/DVD/AMMediaboard.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/EXI/EXI.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/HW/SI/SI_DeviceGCController.h"
#include "Core/HW/Sram.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "Core/Movie.h"
#include "Core/NetPlayProto.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"
#include "Core/WiiRoot.h"
#include "DiscIO/Enums.h"

namespace SerialInterface
{

JVSIOMessage::JVSIOMessage()
{
  m_ptr = 0;
  m_last_start = 0;
}

void JVSIOMessage::start(int node)
{
  m_last_start = m_ptr;
  u8 hdr[3] = {0xE0, (u8)node, 0};
  m_csum = 0;
  addData(hdr, 3, 1);
}

void JVSIOMessage::addData(const u8* dst, size_t len, int sync = 0)
{
  while (len--)
  {
    int c = *dst++;
    if (!sync && ((c == 0xE0) || (c == 0xD0)))
    {
      m_msg[m_ptr++] = 0xD0;
      m_msg[m_ptr++] = c - 1;
    }
    else
    {
      m_msg[m_ptr++] = c;
    }

    if (!sync)
      m_csum += c;
    sync = 0;
    if (m_ptr >= 0x80)
      PanicAlertFmt("JVSIOMessage overrun!");
  }
}

void JVSIOMessage::addData(const void* data, size_t len)
{
  addData((const u8*)data, len);
}

void JVSIOMessage::addData(const char* data)
{
  addData(data, strlen(data));
}

void JVSIOMessage::addData(u32 n)
{
  u8 cs = n;
  addData(&cs, 1);
}

void JVSIOMessage::end()
{
  u32 len = m_ptr - m_last_start;
  m_msg[m_last_start + 2] = len - 2;  // assuming len <0xD0
  addData(m_csum + len - 2);
}

static u8 CheckSumXOR(u8* Data, u32 Length)
{
  u8 check = 0;

  for (u32 i = 0; i < Length; i++)
  {
    check ^= Data[i];
  }

  return check;
}

static u8 last[2][0x80];
static u32 lastptr[2];
/*
  Reply has to be delayed due a bug in the parser
*/
static void swap_buffers(u8* buffer, u32* buffer_length)
{
  memcpy(last[1], buffer, 0x80);   // Save current buffer
  memcpy(buffer, last[0], 0x80);   // Load previous buffer
  memcpy(last[0], last[1], 0x80);  // Update history

  lastptr[1] = *buffer_length;  // Swap lengths
  *buffer_length = lastptr[0];
  lastptr[0] = lastptr[1];
}

static const char s_cdr_program_version[] = {"           Version 1.22,2003/09/19,171-8213B"};
static const char s_cdr_boot_version[] = {"           Version 1.04,2003/06/17,171-8213B"};
static const u8 s_cdr_card_data[] = {
    0x00, 0x6E, 0x00, 0x00, 0x01, 0x00, 0x00, 0x06, 0x00, 0x00, 0x07, 0x00, 0x00, 0x0B,
    0x00, 0x00, 0x0E, 0x00, 0x00, 0x10, 0x00, 0x00, 0x17, 0x00, 0x00, 0x19, 0x00, 0x00,
    0x1A, 0x00, 0x00, 0x1B, 0x00, 0x00, 0x1D, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x20, 0x00,
    0x00, 0x22, 0x00, 0x00, 0x23, 0x00, 0x00, 0x24, 0x00, 0x00, 0x27, 0x00, 0x00, 0x28,
    0x00, 0x00, 0x2C, 0x00, 0x00, 0x2F, 0x00, 0x00, 0x34, 0x00, 0x00, 0x35, 0x00, 0x00,
    0x37, 0x00, 0x00, 0x38, 0x00, 0x00, 0x39, 0x00, 0x00, 0x3D, 0x00};

// AM-Baseboard device on SI
CSIDevice_AMBaseboard::CSIDevice_AMBaseboard(Core::System& system, SIDevices device,
                                             int device_number)
    : ISIDevice(system, device, device_number)
{
  memset(m_coin, 0, sizeof(m_coin));

  // Setup IC-card
  m_ic_card_state = 0x20;
  m_ic_card_status = 0;
  m_ic_card_session = 0x23;

  m_ic_write_size = 0;
  m_ic_write_offset = 0;

  memset(m_ic_write_buffer, 0, sizeof(m_ic_write_buffer));
  memset(m_ic_card_data, 0, sizeof(m_ic_card_data));

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

  // Setup CARD
  m_card_memory_size = 0;
  m_card_is_inserted = 0;

  m_card_offset = 0;
  m_card_command = 0;
  m_card_clean = 0;

  m_card_write_length = 0;
  m_card_wrote = 0;

  m_card_read_length = 0;
  m_card_read = 0;

  m_card_bit = 0;
  m_card_shutter = 1;  // Open
  m_card_state_call_count = 0;

  // Serial
  m_wheelinit = 0;

  m_motorinit = 0;
  m_motorforce_x = 0;

  m_fzdx_seatbelt = 1;
  m_fzdx_motion_stop = 0;
  m_fzdx_sensor_right = 0;
  m_fzdx_sensor_left = 0;

  m_rx_reply = 0xF0;

  m_fzcc_seatbelt = 1;
  m_fzcc_sensor = 0;
  m_fzcc_emergency = 0;
  m_fzcc_service = 0;

  memset(m_motorreply, 0, sizeof(m_motorreply));
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

  u16 crc = CheckSumXOR(iccommand->data + 2, iccommand->pktlen - 1);

  for (u32 i = 0; i < iccommand->pktlen + 1; ++i)
  {
    buffer[(*length)++] = iccommand->data[i];
  }

  buffer[(*length)++] = crc;
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
      u32 id = Common::swap32(SI_AM_BASEBOARD | 0x100);
      std::memcpy(buffer, &id, sizeof(id));
      return sizeof(id);
    }
    break;
    case BaseBoardCommand::GCAM_Command:
    {
      u32 checksum = 0;
      for (u32 i = 0; i < buffer_length; ++i)
        checksum += buffer[i];

      u8 data_out[0x80];
      u32 data_offset = 0;

      static u32 dip_switch_1 = 0xFE;
      static u32 dip_switch_0 = 0xFF;

      memset(data_out, 0, sizeof(data_out));
      data_out[data_offset++] = 1;
      data_out[data_offset++] = 1;

      u8* data_in = buffer + 2;
      u8* data_in_end = buffer + buffer[buffer_position] + 2;

      while (data_in < data_in_end)
      {
        u32 gcam_command = *data_in++;
        switch (GCAMCommand(gcam_command))
        {
        case GCAMCommand::StatusSwitches:
        {
          u8 status = *data_in++;
          DEBUG_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x10, {:02x} (READ STATUS&SWITCHES)",
                        status);

          GCPadStatus PadStatus;
          PadStatus = Pad::GetStatus(ISIDevice::m_device_number);
          data_out[data_offset++] = gcam_command;
          data_out[data_offset++] = 0x2;

          /* baseboard test/service switches
          if (PadStatus.button & PAD_BUTTON_Y)	// Test
            dip_switch_0 &= ~0x80;
          if (PadStatus.button & PAD_BUTTON_X)	// Service
            dip_switch_0 &= ~0x40;
          */

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
          memcpy(data_out + data_offset, "AADE-01B98394904", 16);
          data_offset += 16;
          break;
        }
        case GCAMCommand::Unknown_12:
          NOTICE_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x12, {:02x} {:02x}", *data_in++,
                         *data_in++);
          data_out[data_offset++] = gcam_command;
          data_out[data_offset++] = 0x00;
          break;
        case GCAMCommand::Unknown_14:
          NOTICE_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x14, {:02x} {:02x}", *data_in++,
                         *data_in++);
          data_out[data_offset++] = gcam_command;
          data_out[data_offset++] = 0x00;
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
                         *data_in++, *data_in++, *data_in++, *data_in++, *data_in++);
          u8 string[] = "\x00\x00\x30\x00"
                        //   "\x01\xfe\x00\x00"  // JAPAN
                        "\x02\xfd\x00\x00"  // USA
                        // "\x03\xfc\x00\x00"  // export
                        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff";
          data_out[data_offset++] = gcam_command;
          data_out[data_offset++] = 0x14;

          for (int i = 0; i < 0x14; ++i)
            data_out[data_offset++] = string[i];
        }
        break;
        /* No reply
           Note: Always sends three bytes even though size is set to two
        */
        case GCAMCommand::Unknown_21:
        {
          DEBUG_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x21, {:02x}, {:02x}, {:02x}, {:02x}",
                        data_in[0], data_in[1], data_in[2], data_in[3]);
          data_in += 4;
        }
        break;
        /* No reply
           Note: Always sends six bytes
        */
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
          DEBUG_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x23, {:02x} {:02x}", *data_in++,
                        *data_in++);
          data_out[data_offset++] = gcam_command;
          data_out[data_offset++] = 0x00;
          break;
        case GCAMCommand::Unknown_24:
          DEBUG_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x24, {:02x} {:02x}", *data_in++,
                        *data_in++);
          data_out[data_offset++] = gcam_command;
          data_out[data_offset++] = 0x00;
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

              switch (m_wheelinit)
              {
              case 0:
                data_out[data_offset++] = 'E';  // Error
                data_out[data_offset++] = '0';
                data_out[data_offset++] = '0';
                m_wheelinit++;
                break;
              case 1:
                data_out[data_offset++] = 'C';  // Power Off
                data_out[data_offset++] = '0';
                data_out[data_offset++] = '6';
                // Only turn on when a wheel is connected
                if (serial_interface.GetDeviceType(1) == SerialInterface::SIDEVICE_GC_STEERING)
                {
                  m_wheelinit++;
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
              /*
              u16 CenteringForce= ptr(6);
              u16 FrictionForce = ptr(8);
              u16 Roll          = ptr(10);
              */

              data_in += length;
              break;
            }

            // Serial - Unknown
            if (AMMediaboard::GetGameType() == GekitouProYakyuu)
            {
              u32 serial_command = *(u32*)(data_in);

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
                u32 size = data_in[1];

                char logptr[1024];
                char* log = logptr;

                for (u32 i = 0; i < (u32)(data_in[1] + 2); ++i)
                {
                  log += sprintf(log, "%02X ", data_in[i]);
                }

                INFO_LOG_FMT(SERIALINTERFACE_CARD, "Command: {}", logptr);

                INFO_LOG_FMT(
                    SERIALINTERFACE_CARD,
                    "GC-AM: Command 25 (IC-CARD) Write Pages: Off:{:x} Size:{:x} PSize:{:x}",
                    m_ic_write_offset, m_ic_write_size, size);

                memcpy(m_ic_write_buffer + m_ic_write_offset, data_in + 2, size);

                m_ic_write_offset += size;

                if (m_ic_write_offset > m_ic_write_size)
                {
                  m_ic_write_offset = 0;

                  u16 page = m_ic_write_buffer[5];
                  u16 count = m_ic_write_buffer[7];

                  memcpy(m_ic_card_data + page * 8, m_ic_write_buffer + 10, count * 8);

                  INFO_LOG_FMT(SERIALINTERFACE_CARD,
                               "GC-AM: Command 25 (IC-CARD) Write Pages:{} Count:{}({:x})", page,
                               count, size);

                  icco.command = WritePages;

                  ICCardSendReply(&icco, data_out, &data_offset);
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
                u16 page = Common::swap16(*(u16*)(data_in + 6));

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
                u16 page = Common::swap16(*(u16*)(data_in + 8));

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
                u16 page = Common::swap16(*(u16*)(data_in + 6));

                icco.extlen = 2;
                icco.length += icco.extlen;
                icco.pktlen += icco.extlen;

                *(u16*)(m_ic_card_data + 0x28) = *(u16*)(m_ic_card_data + 0x28) - 1;

                // Counter
                icco.extdata[0] = m_ic_card_data[0x28];
                icco.extdata[1] = m_ic_card_data[0x29];

                INFO_LOG_FMT(SERIALINTERFACE_CARD,
                             "GC-AM: Command 31 (IC-CARD) Decrease Use Count:{}", page);
                break;
              }
              case ICCARDCommand::ReadPages:
              {
                u16 page = Common::swap16(*(u16*)(data_in + 6));
                u16 count = Common::swap16(*(u16*)(data_in + 8));

                u32 offs = page * 8;
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
                u16 pksize = length;
                u16 size = Common::swap16(*(u16*)(data_in + 2));
                u16 page = Common::swap16(*(u16*)(data_in + 6));
                u16 count = Common::swap16(*(u16*)(data_in + 8));

                // We got a complete packet
                if (pksize - 5 == size)
                {
                  if (page == 4)  // Read Only Page, must return error
                  {
                    icco.status = 0x80;
                  }
                  else
                  {
                    memcpy(m_ic_card_data + page * 8, data_in + 13, count * 8);
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

              ICCardSendReply(&icco, data_out, &data_offset);

              data_in += length;
              break;
            }
          }

          u32 command_offset = 0;
          while (command_offset < length)
          {
            // All commands are OR'd with 0x80
            // Last byte is checksum which we don't care about
            u32 serial_command = Common::swap32(*(u32*)(data_in + command_offset));
            serial_command ^= 0x80000000;
            if (AMMediaboard::GetGameType() == FZeroAX ||
                AMMediaboard::GetGameType() == FZeroAXMonster)
            {
              INFO_LOG_FMT(SERIALINTERFACE_AMBB,
                           "GC-AM: Command 0x31 (MOTOR) Length:{:02x} Command:{:06x}({:02x})",
                           length, serial_command >> 8, serial_command & 0xFF);
            }
            else
            {
              INFO_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x31 (SERIAL) Command:{:06x}",
                           serial_command);

              if (/*command == 0xf43200 || */ serial_command == 0x801000)
              {
                // u32 PC = m_system.GetPowerPC().GetPPCState().pc;

                // INFO_LOG_FMT(SERIALINTERFACE_AMBB, "GCAM: PC:{:08x}", PC);

                // m_system.GetPowerPC().GetBreakPoints().Add(PC + 8, true, true, std::nullopt);

                data_out[data_offset++] = 0x31;
                data_out[data_offset++] = 0x02;
                data_out[data_offset++] = 0xFF;
                data_out[data_offset++] = 0x01;
              }
            }

            command_offset += 4;

            if (AMMediaboard::GetGameType() == FZeroAX ||
                AMMediaboard::GetGameType() == FZeroAXMonster)
            {
              // Status
              m_motorreply[command_offset + 2] = 0;
              m_motorreply[command_offset + 3] = 0;

              // Error
              m_motorreply[command_offset + 4] = 0;

              switch (serial_command >> 24)
              {
              case 0:
                break;
              case 1:  // Set Maximum?
                break;
              case 2:
                break;
                /*
                  0x00-0x40: left
                  0x40-0x80: right
                */
              case 4:  // Move Steering Wheel
                // Left
                if (serial_command & 0x010000)
                {
                  m_motorforce_x = -((s16)serial_command & 0xFF00);
                }
                else  // Right
                {
                  m_motorforce_x = (serial_command - 0x4000) & 0xFF00;
                }

                m_motorforce_x *= 2;

                // FFB
                if (m_motorinit == 2)
                {
                  if (serial_interface.GetDeviceType(1) == SerialInterface::SIDEVICE_GC_STEERING)
                  {
                    GCPadStatus PadStatus;
                    PadStatus = Pad::GetStatus(1);
                    if (PadStatus.isConnected)
                    {
                      ControlState mapped_strength = (double)(m_motorforce_x >> 8);
                      mapped_strength /= 127.f;
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
                m_motorinit = 2;
                break;
              // Reset
              case 0x7F:
                m_motorinit = 1;
                memset(m_motorreply, 0, sizeof(m_motorreply));
                break;
              }

              // Checksum
              m_motorreply[command_offset + 5] = m_motorreply[command_offset + 2] ^
                                                 m_motorreply[command_offset + 3] ^
                                                 m_motorreply[command_offset + 4];
            }
          }

          if (length == 0)
          {
            data_out[data_offset++] = gcam_command;
            data_out[data_offset++] = 0x00;
          }
          else
          {
            if (m_motorinit)
            {
              // Motor
              m_motorreply[0] = gcam_command;
              m_motorreply[1] = length;  // Same out as in size

              memcpy(data_out + data_offset, m_motorreply, m_motorreply[1] + 2);
              data_offset += m_motorreply[1] + 2;
            }
          }

          data_in += length;
          break;
        }
        case GCAMCommand::SerialB:
        {
          DEBUG_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 32 (CARD-Interface)");
          u32 length = *data_in++;
          if (length)
          {
            /* Send Card Reply */
            if (length == 1 && data_in[0] == 0x05)
            {
              if (m_card_read_length)
              {
                data_out[data_offset++] = gcam_command;
                u32 ReadLength = m_card_read_length - m_card_read;

                if (AMMediaboard::GetGameType() == FZeroAX)
                {
                  if (ReadLength > 0x2F)
                    ReadLength = 0x2F;
                }

                data_out[data_offset++] = ReadLength;  // 0x2F (max size per packet)

                memcpy(data_out + data_offset, m_card_read_packet + m_card_read, ReadLength);

                data_offset += ReadLength;
                m_card_read += ReadLength;

                if (m_card_read >= m_card_read_length)
                  m_card_read_length = 0;

                data_in += length;
                break;
              }

              data_out[data_offset++] = gcam_command;
              u32 command_length_offset = data_offset;
              data_out[data_offset++] = 0x00;  // len

              data_out[data_offset++] = 0x02;  //
              u32 checksum_start = data_offset;

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
                /*
                  bit 0: Please take your card
                  bit 1: endless waiting causes UNK_E to be called
                */
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
                data_out[data_offset++] = 0x00;  // 0x02
                data_out[data_offset++] = 0x00;  // 0x03
                break;
              case CARDCommand::RegisterFont:
                data_out[data_offset++] = 0x00;  // 0x02
                data_out[data_offset++] = 0x00;  // 0x03
                break;
              case CARDCommand::WriteInfo:
                data_out[data_offset++] = 0x02;  // 0x02
                data_out[data_offset++] = 0x00;  // 0x03
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

              u32 i;
              data_out[data_offset] = 0;  // 0x07
              for (i = 0; i < data_out[checksum_start]; ++i)
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
                        std::string card_filename(File::GetUserPath(D_TRIUSER_IDX) + "tricard_" +
                                                  SConfig::GetInstance().GetTriforceID().c_str() +
                                                  ".bin");

                        if (File::Exists(card_filename))
                        {
                          File::IOFile card = File::IOFile(card_filename, "rb+");
                          m_card_memory_size = (u32)card.GetSize();

                          card.ReadBytes(m_card_memory, m_card_memory_size);
                          card.Close();

                          m_card_is_inserted = 1;
                        }
                      }

                      if (AMMediaboard::GetGameType() == FZeroAX && m_card_memory_size)
                      {
                        m_card_state_call_count++;
                        if (m_card_state_call_count > 10)
                        {
                          if (m_card_bit & 2)
                            m_card_bit &= ~2;
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
                        std::string card_filename(File::GetUserPath(D_TRIUSER_IDX) + "tricard_" +
                                                  SConfig::GetInstance().GetTriforceID().c_str() +
                                                  ".bin");

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
                      u8 mode = m_card_buffer[6];
                      u8 bitmode = m_card_buffer[7];
                      u8 track = m_card_buffer[8];

                      NOTICE_LOG_FMT(SERIALINTERFACE_CARD,
                                     "GC-AM: Command CARD Read({:02X},{:02X},{:02X})", mode,
                                     bitmode, track);

                      // Prepare read packet
                      memset(m_card_read_packet, 0, 0xDB);
                      u32 packet_offset = 0;

                      std::string card_filename(File::GetUserPath(D_TRIUSER_IDX) + "tricard_" +
                                                SConfig::GetInstance().GetTriforceID().c_str() +
                                                ".bin");

                      if (File::Exists(card_filename))
                      {
                        File::IOFile card = File::IOFile(card_filename, "rb+");
                        if (m_card_memory_size == 0)
                        {
                          m_card_memory_size = (u32)card.GetSize();
                        }

                        card.ReadBytes(m_card_memory, m_card_memory_size);
                        card.Close();

                        m_card_is_inserted = 1;
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

                      m_card_read_packet[packet_offset++] = 0x30;  //
                      m_card_read_packet[packet_offset++] = 0x30;  //

                      // Data reply
                      memcpy(m_card_read_packet + packet_offset, m_card_memory, m_card_memory_size);
                      packet_offset += m_card_memory_size;

                      m_card_read_packet[packet_offset++] = 0x03;

                      m_card_read_packet[1] = packet_offset - 1;  // SUB CMDLen

                      u32 i;
                      for (i = 0; i < packet_offset - 1; ++i)
                        m_card_read_packet[packet_offset] ^= m_card_read_packet[1 + i];

                      packet_offset++;

                      m_card_read_length = packet_offset;
                      m_card_read = 0;
                      break;
                    }
                    case CARDCommand::Write:
                    {
                      u8 mode = m_card_buffer[6];
                      u8 bitmode = m_card_buffer[7];
                      u8 track = m_card_buffer[8];

                      m_card_memory_size = m_card_buffer[1] - 9;

                      memcpy(m_card_memory, m_card_buffer + 9, m_card_memory_size);

                      NOTICE_LOG_FMT(SERIALINTERFACE_CARD,
                                     "GC-AM: Command CARD Write: {:02X} {:02X} {:02X} {}", mode,
                                     bitmode, track, m_card_memory_size);

                      std::string card_filename(File::GetUserPath(D_TRIUSER_IDX) + "tricard_" +
                                                SConfig::GetInstance().GetTriforceID().c_str() +
                                                ".bin");

                      File::IOFile card = File::IOFile(card_filename, "wb+");
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
                        m_card_shutter = 0;
                      }
                      // Open
                      else if (m_card_buffer[6] == 0x31)
                      {
                        m_card_shutter = 1;
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

          u8* frame = &data_in[0];
          u8 nr_bytes = frame[3];        // Byte after E0 xx
          u32 frame_len = nr_bytes + 3;  // Header(2) + length byte + payload + checksum

          u8 jvs_buf[0x80];
          memcpy(jvs_buf, frame, frame_len);

          // Extract node and payload pointers
          u8 node = jvs_buf[2];
          u8* jvs_io = jvs_buf + 4;           // First payload byte
          u8* jvs_end = jvs_buf + frame_len;  // One byte before checksum

          message.start(0);
          message.addData(1);

          // Now iterate over the payload
          while (jvs_io < jvs_end)
          {
            int jvsio_command = *jvs_io++;
            DEBUG_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO:node={}, command={:02x}", node,
                          jvsio_command);

            switch (JVSIOCommand(jvsio_command))
            {
            case JVSIOCommand::IOID:
              message.addData(StatusOkay);
              switch (AMMediaboard::GetGameType())
              {
              case FZeroAX:
                // Specific version that enables DX mode on AX machines, all this does is enable the
                // motion of a chair
                message.addData("SEGA ENTERPRISES,LTD.;837-13844-01 I/O CNTL BD2 ;");
                break;
              case FZeroAXMonster:
              case MarioKartGP:
              case MarioKartGP2:
              default:
                message.addData("namco ltd.;FCA-1;Ver1.01;JPN,Multipurpose + Rotary Encoder");
                break;
              case VirtuaStriker3:
              case VirtuaStriker4:
                message.addData("SEGA ENTERPRISES,LTD.;I/O BD JVS;837-13551;Ver1.00");
                break;
              }
              NOTICE_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Command 0x10, BoardID");
              message.addData((u32)0);
              break;
            case JVSIOCommand::CommandRevision:
              message.addData(StatusOkay);
              message.addData(0x11);
              NOTICE_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Command 0x11, CommandRevision");
              break;
            case JVSIOCommand::JVRevision:
              message.addData(StatusOkay);
              message.addData(0x20);
              NOTICE_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Command 0x12, JVRevision");
              break;
            case JVSIOCommand::CommunicationVersion:
              message.addData(StatusOkay);
              message.addData(0x10);
              NOTICE_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Command 0x13, CommunicationVersion");
              break;
              /*
                Slave features:

                Inputs:
                0x01: Switchinput:  players,  buttons
                0x02: Coininput:    slots
                0x03: Analoginput:  channels, bits
                0x04: Rotary input: channels
                0x05: Keycode input: 0,0,0 ?
                0x06: Screen position input: Xbits, Ybits, channels

                Outputs:
                0x10: Card system: slots
                0x11: Medal hopper: channels
                0x12: GPO-out: slots
                0x13: Analog output: channels
                0x14: Character output: width, height, type
                0x15: Backup
              */
            case JVSIOCommand::CheckFunctionality:
              message.addData(StatusOkay);
              switch (AMMediaboard::GetGameType())
              {
              case FZeroAX:
              case FZeroAXMonster:
                // 2 Player (12bit) (p2=paddles), 1 Coin slot, 6 Analog-in
                // message.addData((void *)"\x01\x02\x0C\x00", 4);
                // message.addData((void *)"\x02\x01\x00\x00", 4);
                // message.addData((void *)"\x03\x06\x00\x00", 4);
                // message.addData((void *)"\x00\x00\x00\x00", 4);
                //
                // DX Version: 2 Player (22bit) (p2=paddles), 2 Coin slot, 8 Analog-in,
                // 22 Driver-out
                message.addData((void*)"\x01\x02\x12\x00", 4);
                message.addData((void*)"\x02\x02\x00\x00", 4);
                message.addData((void*)"\x03\x08\x0A\x00", 4);
                message.addData((void*)"\x12\x16\x00\x00", 4);
                message.addData((void*)"\x00\x00\x00\x00", 4);
                break;
              case VirtuaStriker3:
              case GekitouProYakyuu:
                // 2 Player (13bit), 2 Coin slot, 4 Analog-in, 1 CARD, 8 Driver-out
                message.addData((void*)"\x01\x02\x0D\x00", 4);
                message.addData((void*)"\x02\x02\x00\x00", 4);
                message.addData((void*)"\x03\x04\x00\x00", 4);
                message.addData((void*)"\x10\x01\x00\x00", 4);
                message.addData((void*)"\x12\x08\x00\x00", 4);
                message.addData((void*)"\x00\x00\x00\x00", 4);
                break;
              case VirtuaStriker4:
                // 2 Player (13bit), 1 Coin slot, 4 Analog-in, 1 CARD
                message.addData((void*)"\x01\x02\x0D\x00", 4);
                message.addData((void*)"\x02\x01\x00\x00", 4);
                message.addData((void*)"\x03\x04\x00\x00", 4);
                message.addData((void*)"\x10\x01\x00\x00", 4);
                message.addData((void*)"\x00\x00\x00\x00", 4);
                break;
              case KeyOfAvalon:
                // 1 Player (15bit), 1 Coin slot, 3 Analog-in, Touch, 1 CARD, 1 Driver-out
                // (Unconfirmed)
                message.addData((void*)"\x01\x01\x0F\x00", 4);
                message.addData((void*)"\x02\x01\x00\x00", 4);
                message.addData((void*)"\x03\x03\x00\x00", 4);
                message.addData((void*)"\x06\x10\x10\x01", 4);
                message.addData((void*)"\x10\x01\x00\x00", 4);
                message.addData((void*)"\x12\x01\x00\x00", 4);
                message.addData((void*)"\x00\x00\x00\x00", 4);
                break;
              case MarioKartGP:
              case MarioKartGP2:
              default:
                // 1 Player (15bit), 1 Coin slot, 3 Analog-in, 1 CARD, 1 Driver-out
                message.addData((void*)"\x01\x01\x0F\x00", 4);
                message.addData((void*)"\x02\x01\x00\x00", 4);
                message.addData((void*)"\x03\x03\x00\x00", 4);
                message.addData((void*)"\x10\x01\x00\x00", 4);
                message.addData((void*)"\x12\x01\x00\x00", 4);
                message.addData((void*)"\x00\x00\x00\x00", 4);
                break;
              }
              NOTICE_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Command 0x14, CheckFunctionality");
              break;
            case JVSIOCommand::MainID:
              while (*jvs_io++)
              {
              };
              message.addData(StatusOkay);
              break;
            case JVSIOCommand::SwitchesInput:
            {
              int player_count = *jvs_io++;
              int player_byte_count = *jvs_io++;

              DEBUG_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO:  Command 0x20, SwitchInputs: {} {}",
                            player_count, player_byte_count);

              message.addData(StatusOkay);

              GCPadStatus PadStatus;
              PadStatus = Pad::GetStatus(0);

              // Test button
              if (PadStatus.button & PAD_TRIGGER_Z)
                message.addData(0x80);
              else
                message.addData((u32)0x00);

              for (int i = 0; i < player_count; ++i)
              {
                unsigned char player_data[3] = {0, 0, 0};

                switch (AMMediaboard::GetGameType())
                {
                // Controller configuration for F-Zero AX (DX)
                case FZeroAX:
                  PadStatus = Pad::GetStatus(0);
                  if (i == 0)
                  {
                    if (m_fzdx_seatbelt)
                    {
                      player_data[0] |= 0x01;
                    }

                    // Start
                    if (PadStatus.button & PAD_BUTTON_START)
                      player_data[0] |= 0x80;
                    // Service button
                    if (PadStatus.button & PAD_BUTTON_X)
                      player_data[0] |= 0x40;
                    // Boost
                    if (PadStatus.button & PAD_BUTTON_Y)
                      player_data[0] |= 0x02;
                    // View Change 1
                    if (PadStatus.button & PAD_BUTTON_RIGHT)
                      player_data[0] |= 0x20;
                    // View Change 2
                    if (PadStatus.button & PAD_BUTTON_LEFT)
                      player_data[0] |= 0x10;
                    // View Change 3
                    if (PadStatus.button & PAD_BUTTON_UP)
                      player_data[0] |= 0x08;
                    // View Change 4
                    if (PadStatus.button & PAD_BUTTON_DOWN)
                      player_data[0] |= 0x04;
                    player_data[1] = m_rx_reply & 0xF0;
                  }
                  else if (i == 1)
                  {
                    //  Paddle left
                    if (PadStatus.button & PAD_BUTTON_A)
                      player_data[0] |= 0x20;
                    //  Paddle right
                    if (PadStatus.button & PAD_BUTTON_B)
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
                  PadStatus = Pad::GetStatus(0);
                  if (i == 0)
                  {
                    if (m_fzcc_sensor)
                    {
                      player_data[0] |= 0x01;
                    }

                    // Start
                    if (PadStatus.button & PAD_BUTTON_START)
                      player_data[0] |= 0x80;
                    // Service button
                    if (PadStatus.button & PAD_BUTTON_X)
                      player_data[0] |= 0x40;
                    // Boost
                    if (PadStatus.button & PAD_BUTTON_Y)
                      player_data[0] |= 0x02;
                    // View Change 1
                    if (PadStatus.button & PAD_BUTTON_RIGHT)
                      player_data[0] |= 0x20;
                    // View Change 2
                    if (PadStatus.button & PAD_BUTTON_LEFT)
                      player_data[0] |= 0x10;
                    // View Change 3
                    if (PadStatus.button & PAD_BUTTON_UP)
                      player_data[0] |= 0x08;
                    // View Change 4
                    if (PadStatus.button & PAD_BUTTON_DOWN)
                      player_data[0] |= 0x04;

                    player_data[1] = m_rx_reply & 0xF0;
                  }
                  else if (i == 1)
                  {
                    //  Paddle left
                    if (PadStatus.button & PAD_BUTTON_A)
                      player_data[0] |= 0x20;
                    //  Paddle right
                    if (PadStatus.button & PAD_BUTTON_B)
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
                  PadStatus = Pad::GetStatus(i);
                  // Start
                  if (PadStatus.button & PAD_BUTTON_START)
                    player_data[0] |= 0x80;
                  // Service button
                  if (PadStatus.button & PAD_BUTTON_X)
                    player_data[0] |= 0x40;
                  // Long Pass
                  if (PadStatus.button & PAD_TRIGGER_L)
                    player_data[0] |= 0x01;
                  // Short Pass
                  if (PadStatus.button & PAD_TRIGGER_R)
                    player_data[1] |= 0x80;
                  // Shoot
                  if (PadStatus.button & PAD_BUTTON_A)
                    player_data[0] |= 0x02;
                  // Left
                  if (PadStatus.button & PAD_BUTTON_LEFT)
                    player_data[0] |= 0x08;
                  // Up
                  if (PadStatus.button & PAD_BUTTON_UP)
                    player_data[0] |= 0x20;
                  // Right
                  if (PadStatus.button & PAD_BUTTON_RIGHT)
                    player_data[0] |= 0x04;
                  // Down
                  if (PadStatus.button & PAD_BUTTON_DOWN)
                    player_data[0] |= 0x10;
                  break;
                // Controller configuration for Virtua Striker 4 games
                case VirtuaStriker4:
                {
                  PadStatus = Pad::GetStatus(i);
                  // Start
                  if (PadStatus.button & PAD_BUTTON_START)
                    player_data[0] |= 0x80;
                  // Service button
                  if (PadStatus.button & PAD_BUTTON_X)
                    player_data[0] |= 0x40;
                  // Long Pass
                  if (PadStatus.button & PAD_TRIGGER_L)
                    player_data[0] |= 0x01;
                  // Short Pass
                  if (PadStatus.button & PAD_TRIGGER_R)
                    player_data[0] |= 0x02;
                  // Shoot
                  if (PadStatus.button & PAD_BUTTON_A)
                    player_data[1] |= 0x80;
                  // Dash
                  if (PadStatus.button & PAD_BUTTON_B)
                    player_data[1] |= 0x40;
                  // Tactics (U)
                  if (PadStatus.button & PAD_BUTTON_LEFT)
                    player_data[0] |= 0x20;
                  // Tactics (M)
                  if (PadStatus.button & PAD_BUTTON_UP)
                    player_data[0] |= 0x08;
                  // Tactics (D)
                  if (PadStatus.button & PAD_BUTTON_RIGHT)
                    player_data[0] |= 0x04;

                  if (i == 0)
                  {
                    player_data[0] |= 0x10;  // IC-Card Switch ON

                    // IC-Card Lock
                    if (PadStatus.button & PAD_BUTTON_DOWN)
                      player_data[1] |= 0x20;
                  }
                }
                break;
                // Controller configuration for Gekitou Pro Yakyuu
                case GekitouProYakyuu:
                  PadStatus = Pad::GetStatus(i);
                  // Start
                  if (PadStatus.button & PAD_BUTTON_START)
                    player_data[0] |= 0x80;
                  // Service button
                  if (PadStatus.button & PAD_BUTTON_X)
                    player_data[0] |= 0x40;
                  //  A
                  if (PadStatus.button & PAD_BUTTON_B)
                    player_data[0] |= 0x01;
                  //  B
                  if (PadStatus.button & PAD_BUTTON_A)
                    player_data[0] |= 0x02;
                  //  Gekitou
                  if (PadStatus.button & PAD_TRIGGER_L)
                    player_data[1] |= 0x80;
                  // Left
                  if (PadStatus.button & PAD_BUTTON_LEFT)
                    player_data[0] |= 0x08;
                  // Up
                  if (PadStatus.button & PAD_BUTTON_UP)
                    player_data[0] |= 0x20;
                  // Right
                  if (PadStatus.button & PAD_BUTTON_RIGHT)
                    player_data[0] |= 0x04;
                  // Down
                  if (PadStatus.button & PAD_BUTTON_DOWN)
                    player_data[0] |= 0x10;
                  break;
                // Controller configuration for Mario Kart and other games
                default:
                case MarioKartGP:
                case MarioKartGP2:
                {
                  PadStatus = Pad::GetStatus(0);
                  // Start
                  if (PadStatus.button & PAD_BUTTON_START)
                    player_data[0] |= 0x80;
                  // Service button
                  if (PadStatus.button & PAD_BUTTON_X)
                    player_data[0] |= 0x40;
                  // Item button
                  if (PadStatus.button & PAD_BUTTON_A)
                    player_data[1] |= 0x20;
                  // VS-Cancel button
                  if (PadStatus.button & PAD_BUTTON_B)
                    player_data[1] |= 0x02;
                }
                break;
                case KeyOfAvalon:
                {
                  PadStatus = Pad::GetStatus(0);
                  // Debug On
                  if (PadStatus.button & PAD_BUTTON_START)
                    player_data[0] |= 0x80;
                  // Service button
                  if (PadStatus.button & PAD_BUTTON_X)
                    player_data[0] |= 0x40;
                  // Switch 1
                  if (PadStatus.button & PAD_BUTTON_A)
                    player_data[0] |= 0x04;
                  // Switch 2
                  if (PadStatus.button & PAD_BUTTON_B)
                    player_data[0] |= 0x08;
                  // Toggle inserted card
                  if (PadStatus.button & PAD_TRIGGER_L)
                  {
                    m_ic_card_status ^= 0x8000;
                  }
                }
                break;
                }

                for (int j = 0; j < player_byte_count; ++j)
                  message.addData(player_data[j]);
              }
              break;
            }
            case JVSIOCommand::CoinInput:
            {
              int slots = *jvs_io++;
              message.addData(StatusOkay);
              for (int i = 0; i < slots; i++)
              {
                GCPadStatus PadStatus;
                PadStatus = Pad::GetStatus(i);
                if ((PadStatus.button & PAD_TRIGGER_Z) && !m_coin_pressed[i])
                {
                  m_coin[i]++;
                }
                m_coin_pressed[i] = PadStatus.button & PAD_TRIGGER_Z;
                message.addData((m_coin[i] >> 8) & 0x3f);
                message.addData(m_coin[i] & 0xff);
              }
              DEBUG_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Command 0x21, CoinInput: {}", slots);
              break;
            }
            case JVSIOCommand::AnalogInput:
            {
              message.addData(StatusOkay);

              int analogs = *jvs_io++;
              GCPadStatus PadStatus;
              GCPadStatus PadStatus2;
              PadStatus = Pad::GetStatus(0);

              DEBUG_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Command 0x22, AnalogInput: {}",
                            analogs);

              switch (AMMediaboard::GetGameType())
              {
              case FZeroAX:
              case FZeroAXMonster:
                // Steering
                if (m_motorinit == 1)
                {
                  if (m_motorforce_x > 0)
                  {
                    message.addData(0x80 - (m_motorforce_x >> 8));
                  }
                  else
                  {
                    message.addData((m_motorforce_x >> 8));
                  }
                  message.addData((u8)0);

                  message.addData(PadStatus.stickY);
                  message.addData((u8)0);
                }
                else
                {
                  message.addData(PadStatus.stickX);
                  message.addData((u8)0);

                  message.addData(PadStatus.stickY);
                  message.addData((u8)0);
                }

                // Unused
                message.addData((u8)0);
                message.addData((u8)0);
                message.addData((u8)0);
                message.addData((u8)0);

                // Gas
                message.addData(PadStatus.triggerRight);
                message.addData((u8)0);

                // Brake
                message.addData(PadStatus.triggerLeft);
                message.addData((u8)0);

                message.addData((u8)0x80);  // Motion Stop
                message.addData((u8)0);

                message.addData((u8)0);
                message.addData((u8)0);

                break;
              case VirtuaStriker3:
              case VirtuaStriker4:
              {
                PadStatus2 = Pad::GetStatus(1);

                message.addData(PadStatus.stickX);
                message.addData((u8)0);
                message.addData(PadStatus.stickY);
                message.addData((u8)0);

                message.addData(PadStatus2.stickX);
                message.addData((u8)0);
                message.addData(PadStatus2.stickY);
                message.addData((u8)0);
              }
              break;
              default:
              case MarioKartGP:
              case MarioKartGP2:
                // Steering
                message.addData(PadStatus.stickX);
                message.addData((u8)0);

                // Gas
                message.addData(PadStatus.triggerRight);
                message.addData((u8)0);

                // Brake
                message.addData(PadStatus.triggerLeft);
                message.addData((u8)0);
                break;
              }
              break;
            }
            case JVSIOCommand::PositionInput:
            {
              int channel = *jvs_io++;

              GCPadStatus PadStatus;
              PadStatus = Pad::GetStatus(0);

              if (PadStatus.button & PAD_TRIGGER_R)
              {
                // Tap at center of screen (~320,240)
                message.addData((void*)"\x01\x00\x8C\x01\x95",
                                5);  // X=320 (0x0140), Y=240 (0x00F0)
              }
              else
              {
                message.addData((void*)"\x01\xFF\xFF\xFF\xFF", 5);
              }

              DEBUG_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Command 0x25, PositionInput:{}",
                            channel);
              break;
            }
            case JVSIOCommand::CoinSubOutput:
            {
              u32 slot = *jvs_io++;
              m_coin[slot] -= (*jvs_io++ << 8) | *jvs_io++;
              message.addData(StatusOkay);
              DEBUG_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Command 0x30, CoinSubOutput: {}", slot);
              break;
            }
            case JVSIOCommand::GeneralDriverOutput:
            {
              u32 bytes = *jvs_io++;

              if (bytes)
              {
                message.addData(StatusOkay);

                // The lamps are controlled via this
                if (AMMediaboard::GetGameType() == MarioKartGP)
                {
                  u32 status = *jvs_io++;
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

                u8* buf = new u8[bytes];

                for (u32 i = 0; i < bytes; ++i)
                {
                  buf[i] = *jvs_io++;
                }

                INFO_LOG_FMT(
                    SERIALINTERFACE_JVSIO,
                    "JVS-IO: Command 0x32, GPO: {:02x} {:02x} {} {:02x}{:02x}{:02x} ({:02x})",
                    delay, m_rx_reply, bytes, buf[0], buf[1], buf[2],
                    Common::swap16(*(u16*)(buf + 1)) >> 2);

                // TODO: figure this out

                u8 trepl[] = {
                    0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xA0, 0xB0, 0xC0,
                    0xD0, 0xE0, 0xF0, 0x01, 0x11, 0x21, 0x31, 0x41, 0x51, 0x61, 0x71, 0x81, 0x91,
                    0xA1, 0xB1, 0xC1, 0xD1, 0xE1, 0xF1, 0x02, 0x12, 0x22, 0x32, 0x42, 0x52, 0x62,
                    0x72, 0x82, 0x92, 0xA2, 0xB2, 0xC2, 0xD2, 0xE2, 0xF2, 0x04, 0x14, 0x24, 0x34,
                    0x44, 0x54, 0x64, 0x74, 0x84, 0x94, 0xA4, 0xB4, 0xC4, 0xD4, 0xE4, 0xF4, 0x05,
                    0x15, 0x25, 0x35, 0x45, 0x55, 0x65, 0x75, 0x85, 0x95, 0xA5, 0xB5, 0xC5, 0xD5,
                    0xE5, 0xF5, 0x06, 0x16, 0x26, 0x36, 0x46, 0x56, 0x66, 0x76, 0x86, 0x96, 0xA6,
                    0xB6, 0xC6, 0xD6, 0xE6, 0xF6, 0x08, 0x18, 0x28, 0x38, 0x48, 0x58, 0x68, 0x78,
                    0x88, 0x98, 0xA8, 0xB8, 0xC8, 0xD8, 0xE8, 0xF8, 0x09, 0x19, 0x29, 0x39, 0x49,
                    0x59, 0x69, 0x79, 0x89, 0x99, 0xA9, 0xB9, 0xC9, 0xD9, 0xE9, 0xF9, 0x0A, 0x1A,
                    0x2A, 0x3A, 0x4A, 0x5A, 0x6A, 0x7A, 0x8A, 0x9A, 0xAA, 0xBA, 0xCA, 0xDA, 0xEA,
                    0xFA, 0x0C, 0x1C, 0x2C, 0x3C, 0x4C, 0x5C, 0x6C, 0x7C, 0x8C, 0x9C, 0xAC, 0xBC,
                    0xCC, 0xDC, 0xEC, 0xFC, 0x0D, 0x1D, 0x2D, 0x3D, 0x4D, 0x5D, 0x6D, 0x7D, 0x8D,
                    0x9D, 0xAD, 0xBD, 0xCD, 0xDD, 0xED, 0xFD, 0x0E, 0x1E, 0x2E, 0x3E, 0x4E, 0x5E,
                    0x6E, 0x7E, 0x8E, 0x9E, 0xAE, 0xBE, 0xCE, 0xDE, 0xEE, 0xFE};

                static u32 off = 0;
                if (off > sizeof(trepl))
                  off = 0;

                switch (Common::swap16(*(u16*)(buf + 1)) >> 2)
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
                ////if( buf[1] == 1 && buf[2] == 0x80 )
                ////{
                ////  INFO_LOG_FMT(DVDINTERFACE, "GCAM: PC:{:08x}", PC);
                ////  PowerPC::breakpoints.Add( PC+8, false );
                ////}

                delete[] buf;
              }
              break;
            }
            case JVSIOCommand::CoinAddOutput:
            {
              int slot = *jvs_io++;
              m_coin[slot] += (*jvs_io++ << 8) | *jvs_io++;
              message.addData(StatusOkay);
              DEBUG_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Command 0x35, CoinAddOutput: {}", slot);
              break;
            }
            case JVSIOCommand::NAMCOCommand:
            {
              int cmd_ = *jvs_io++;
              if (cmd_ == 0x18)
              {  // id check
                jvs_io += 4;
                message.addData(StatusOkay);
                message.addData(0xff);
              }
              else
              {
                message.addData(StatusOkay);
                ERROR_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO:Unknown:{:02x}", cmd_);
              }
              break;
            }
            case JVSIOCommand::Reset:
              if (*jvs_io++ == 0xD9)
              {
                NOTICE_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Command 0xF0, Reset");
                delay = 0;
                m_wheelinit = 0;
                m_ic_card_state = 0x20;
              }
              message.addData(StatusOkay);

              dip_switch_1 |= 1;
              break;
            case JVSIOCommand::SetAddress:
              node = *jvs_io++;
              NOTICE_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Command 0xF1, SetAddress: node={}",
                             node);
              message.addData(node == 1);
              dip_switch_1 &= ~1;
              break;
            default:
              ERROR_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Unhandled: node={}, command={:02x}",
                            node, jvsio_command);
              break;
            }
          }

          message.end();

          data_out[data_offset++] = gcam_command;

          u8* buf = message.m_msg;
          u32 len = message.m_ptr;
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
      char logptr[1024];
      char* log = logptr;

      for (int i = 0; i < 0x7F; ++i)
      {
        checksum += data_in[i] = data_out[i];
        log += sprintf(log, "%02X", data_in[i]);
      }
      data_in[0x7f] = ~checksum;
      DEBUG_LOG_FMT(SERIALINTERFACE_AMBB, "Command send back: {}", logptr);

      swap_buffers(buffer, &buffer_length);

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

// Unused
DataResponse CSIDevice_AMBaseboard::GetData(u32& _Hi, u32& _Low)
{
  _Low = 0;
  _Hi = 0x00800000;

  return DataResponse::Success;
}

void CSIDevice_AMBaseboard::SendCommand(u32 _Cmd, u8 _Poll)
{
  ERROR_LOG_FMT(SERIALINTERFACE, "Unknown direct command     (0x{})", _Cmd);
  PanicAlertFmt("SI: (GCAM) Unknown direct command");
}

}  // namespace SerialInterface
