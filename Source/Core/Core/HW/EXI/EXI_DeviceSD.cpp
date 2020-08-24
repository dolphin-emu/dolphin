// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/EXI/EXI_DeviceSD.h"

#include <string>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Hash.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"

namespace ExpansionInterface
{
CEXISD::CEXISD(Core::System& system) : IEXIDevice(system)
{
  const std::string filename = File::GetUserPath(D_GCUSER_IDX) + "sdcard.bin";
  m_card.Open(filename, "r+b");
  if (!m_card)
  {
    WARN_LOG_FMT(EXPANSIONINTERFACE,
                 "Failed to open SD Card image, trying to create a new 128 MB image...");
    m_card.Open(filename, "wb");
    // NOTE: Not using Common::SDCardCreate here yet, to test games formatting the card
    // themselves.
    if (m_card)
    {
      m_card.Resize(0x8000000);
      INFO_LOG_FMT(EXPANSIONINTERFACE, "Successfully created {}", filename);
      m_card.Open(filename, "r+b");
    }
    if (!m_card)
    {
      ERROR_LOG_FMT(EXPANSIONINTERFACE,
                    "Could not open SD Card image or create a new one, are you running "
                    "from a read-only directory?");
    }
  }
}

void CEXISD::ImmWrite(u32 data, u32 size)
{
  if (inited)
  {
    while (size--)
    {
      u8 byte = data >> 24;
      WriteByte(byte);
      data <<= 8;
    }
  }
  else if (size == 2 && data == 0)
  {
    // Get ID command
    INFO_LOG_FMT(EXPANSIONINTERFACE, "SD: EXI_GetID detected (size = {:x}, data = {:x})", size,
                 data);
    get_id = true;
  }
}

u32 CEXISD::ImmRead(u32 size)
{
  if (get_id)
  {
    // This is not a good way of handling state
    inited = true;
    get_id = false;
    INFO_LOG_FMT(EXPANSIONINTERFACE, "SD: EXI_GetID finished (size = {:x})", size);
    // Same signed/unsigned mismatch in libogc; it wants -1
    return -1;
  }
  else
  {
    u32 res = 0;
    u32 position = 0;
    while (size--)
    {
      u8 byte = ReadByte();
      res |= byte << (24 - (position++ * 8));
    }
    return res;
  }
}

void CEXISD::ImmReadWrite(u32& data, u32 size)
{
  ImmWrite(data, size);
  data = ImmRead(size);
}

void CEXISD::SetCS(int cs)
{
  INFO_LOG_FMT(EXPANSIONINTERFACE, "EXI SD SetCS: {}", cs);
}

bool CEXISD::IsPresent() const
{
  return true;
}

void CEXISD::DoState(PointerWrap& p)
{
  p.Do(inited);
  p.Do(get_id);
  p.Do(next_is_appcmd);
  p.Do(command_position);
  p.Do(block_position);
  p.DoArray(command_buffer);
  p.Do(response);
  p.DoArray(block_buffer);
}

void CEXISD::WriteByte(u8 byte)
{
  // TODO: Write-protect inversion(?)
  if (command_position == 0)
  {
    if ((byte & 0b11000000) == 0b01000000)
    {
      INFO_LOG_FMT(EXPANSIONINTERFACE, "EXI SD command started: {:02x}", byte);
      command_buffer[command_position++] = byte;
    }
  }
  else if (command_position < 6)
  {
    command_buffer[command_position++] = byte;

    if (command_position == 6)
    {
      // Buffer now full
      command_position = 0;

      u8 hash = (Common::HashCrc7(command_buffer.data(), 5) << 1) | 1;
      if (byte != hash)
      {
        WARN_LOG_FMT(EXPANSIONINTERFACE,
                     "EXI SD command invalid, incorrect CRC7 or missing end bit: got {:02x}, "
                     "should be {:02x}",
                     byte, hash);
        response.push_back(static_cast<u8>(R1::CommunicationCRCError));
        return;
      }

      u8 command = command_buffer[0] & 0x3f;
      u32 argument = command_buffer[1] << 24 | command_buffer[2] << 16 | command_buffer[3] << 8 |
                     command_buffer[4];

      INFO_LOG_FMT(EXPANSIONINTERFACE, "EXI SD command received: {:02x} {:08x}", command, argument);

      if (next_is_appcmd)
      {
        next_is_appcmd = false;
        HandleAppCommand(static_cast<AppCommand>(command), argument);
      }
      else
      {
        HandleCommand(static_cast<Command>(command), argument);
      }
    }
  }
}

void CEXISD::HandleCommand(Command command, u32 argument)
{
  switch (command)
  {
  case Command::GoIdleState:
    response.push_back(static_cast<u8>(R1::InIdleState));
    break;
  case Command::SendOpCond:
  {
    // Used by libogc for non-SDHC cards
    bool hcs = argument & (1 << 30);  // Host Capacity Support (for SDHC/SDXC cards)
    (void)hcs;
    response.push_back(0);  // R1 - not idle
    break;
  }
  case Command::SendInterfaceCond:
  {
    u8 supply_voltage = (argument >> 8) & 0xf;
    u8 check_pattern = argument & 0xff;
    // Format R7
    response.push_back(static_cast<u8>(R1::InIdleState));  // R1
    response.push_back(0);                                 // Command version nybble (0), reserved
    response.push_back(0);                                 // Reserved
    response.push_back(supply_voltage);                    // Reserved + voltage
    response.push_back(check_pattern);
    break;
  }
  case Command::SendCSD:
  {
    u64 size = m_card.GetSize();

    // 2048 bytes/sector
    // We could make this dynamic to support a wider range of file sizes
    constexpr u32 read_bl_len = 11;

    // size = (c_size + 1) * (1 << (2 + c_size_mult + read_bl_len))
    u32 c_size_mult = 0;
    bool invalid_size = false;
    while (size > 4096)
    {
      invalid_size |= size & 1;
      size >>= 1;
      if (++c_size_mult >= 8 + 2 + read_bl_len)
      {
        ERROR_LOG_FMT(IOS_SD, "SD Card is too big!");
        // Set max values
        size = 4096;
        c_size_mult = 7 + 2 + read_bl_len;
      }
    }
    c_size_mult -= 2 + read_bl_len;
    --size;
    const u32 c_size(size);

    if (invalid_size)
      WARN_LOG_FMT(IOS_SD, "SD Card size is invalid");
    else
      INFO_LOG_FMT(IOS_SD, "SD C_SIZE = {}, C_SIZE_MULT = {}", c_size, c_size_mult);

    // R1
    response.push_back(0);
    // Data ready token
    response.push_back(0xfe);
    // CSD
    // 0b00           CSD_STRUCTURE (SDv1)
    // 0b000000       reserved
    // 0b01111111     TAAC (8.0 * 10ms)
    // 0b00000000     NSAC
    // 0b00110010     TRAN_SPEED (2.5 * 10 Mbit/s, max operating frequency)

    // 0b010110110101 CCC
    // 0b1111         READ_BL_LEN (2048 bytes)
    // 0b1            READ_BL_PARTIAL
    // 0b0            WRITE_BL_MISALIGN
    // 0b0            READ_BLK_MISALIGN
    // 0b0            DSR_IMP (no driver stage register implemented)
    // 0b00           reserved
    // 0b??????????   C_SIZE (most significant 10 bits)

    // 0b??           C_SIZE (least significant 2 bits)
    // 0b111          VDD_R_CURR_MIN (100 mA)
    // 0b111          VDD_R_CURR_MAX (100 mA)
    // 0b111          VDD_W_CURR_MIN (100 mA)
    // 0b111          VDD_W_CURR_MAX (100 mA)
    // 0b???          C_SIZE_MULT
    // 0b1            ERASE_BLK_EN (erase unit = 512 bytes)
    // 0b1111111      SECTOR_SIZE (128 write blocks)
    // 0b0000000      WP_GRP_SIZE

    // 0b0            WP_GRP_ENABLE (no write protection)
    // 0b00           reserved
    // 0b001          R2W_FACTOR (write half as fast as read)
    // 0b1111         WRITE_BL_LEN (= READ_BL_LEN)
    // 0b0            WRITE_BL_PARTIAL (no partial block writes)
    // 0b00000        reserved
    // 0b0            FILE_FORMAT_GRP (default)
    // 0b1            COPY (contents are copied)
    // 0b0            PERM_WRITE_PROTECT (not permanently write protected)
    // 0b0            TMP_READ_PROTECT (not temporarily write protected)
    // 0b00           FILE_FORMAT (contains partition table)
    // 0b00           reserved
    // 0b???????      CRC
    // 0b1            reserved

    // TODO: CRC7 (but so far it looks like nobody is actually verifying this)
    constexpr u32 crc = 0;

    // Form the csd using the description above
    std::array<u8, 16> csd = {
        0x00,
        0x07,
        0xf0,
        0x03,
        0x5b,
        0x5f,
        static_cast<u8>(0x80 | (c_size >> 10)),
        static_cast<u8>(c_size >> 2),
        static_cast<u8>(0x3f | (c_size << 6)),
        static_cast<u8>(0xfc | (c_size_mult >> 1)),
        static_cast<u8>(0x7f | (c_size << 7)),
        0x80,
        0x07,
        0xc0,
        0x40,
        static_cast<u8>(0x01 | (crc << 1)),
    };
    for (auto byte : csd)
      response.push_back(byte);

    u16 crc16 = Common::HashCrc16(csd);
    response.push_back(crc16 >> 8);
    response.push_back(crc16);
    break;
  }
  case Command::SendCID:
  {
    // R1
    response.push_back(0);
    // Data ready token
    response.push_back(0xfe);
    // The CID -- no idea what the format is, copied from SDIOSlot0
    std::array<u8, 16> cid = {
        0x80, 0x11, 0x4d, 0x1c, 0x80, 0x08, 0x00, 0x00,
        0x80, 0x07, 0xb5, 0x20, 0x80, 0x08, 0x00, 0x00,
    };
    for (auto byte : cid)
      response.push_back(byte);

    u16 crc16 = Common::HashCrc16(cid);
    response.push_back(crc16 >> 8);
    response.push_back(crc16);
    break;
  }
  case Command::StopTransmission:
    response.push_back(0);  // R1
    // There can be further padding bytes, but it's not needed
    break;
  case Command::SetBlockLen:
    INFO_LOG_FMT(EXPANSIONINTERFACE, "Set blocklen to {}", argument);
    // TODO: error if blocklen not 512
    response.push_back(0);  // R1
    break;
  case Command::AppCmd:
    next_is_appcmd = true;
    response.push_back(0);  // R1
    break;
  default:
    // Don't know it
    WARN_LOG_FMT(EXPANSIONINTERFACE, "Unimplemented SD command {:02x} {:08x}",
                 static_cast<u8>(command), argument);
    response.push_back(static_cast<u8>(R1::IllegalCommand));
  }
}

void CEXISD::HandleAppCommand(AppCommand app_command, u32 argument)
{
  switch (app_command)
  {
  case AppCommand::SDStatus:
  {
    response.push_back(0);     // R1
    response.push_back(0);     // R2
    response.push_back(0xfe);  // Data ready token
    // All-zero for now
    std::array<u8, 64> status = {};
    for (auto byte : status)
    {
      response.push_back(byte);
    }

    u16 crc16 = Common::HashCrc16(status);
    response.push_back(crc16 >> 8);
    response.push_back(crc16);
    break;
  }
  case AppCommand::SDSendOpCond:
  {
    // Used by Pokémon Channel for all cards, and libogc for SDHC cards
    bool hcs = argument & (1 << 30);  // Host Capacity Support (for SDHC/SDXC cards)
    (void)hcs;
    response.push_back(0);  // R1 - not idle
    break;
  }
  default:
    // Don't know it
    WARN_LOG_FMT(EXPANSIONINTERFACE, "Unimplemented SD app command {:02x} {:08x}",
                 static_cast<u8>(app_command), argument);
    response.push_back(static_cast<u8>(R1::IllegalCommand));
  }
}

u8 CEXISD::ReadByte()
{
  if (response.empty())
  {
    // WARN_LOG_FMT(EXPANSIONINTERFACE, "Attempted to read from empty SD queue");
    return 0xFF;
  }
  else
  {
    u8 result = response.front();
    response.pop_front();
    return result;
  }
}
}  // namespace ExpansionInterface
