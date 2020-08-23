// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/EXI/EXI_DeviceSD.h"

#include <string>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Hash.h"
#include "Common/Logging/Log.h"

namespace ExpansionInterface
{
CEXISD::CEXISD(Core::System& system) : IEXIDevice(system)
{
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
  p.Do(m_uPosition);
  p.DoArray(cmd);
  p.Do(response);
}

void CEXISD::WriteByte(u8 byte)
{
  // TODO: Write-protect inversion(?)
  if (m_uPosition == 0)
  {
    if ((byte & 0b11000000) == 0b01000000)
    {
      INFO_LOG_FMT(EXPANSIONINTERFACE, "EXI SD command started: {:02x}", byte);
      cmd[m_uPosition++] = byte;
    }
  }
  else if (m_uPosition < 6)
  {
    cmd[m_uPosition++] = byte;

    if (m_uPosition == 6)
    {
      // Buffer now full
      m_uPosition = 0;

      if ((byte & 1) != 1)
      {
        INFO_LOG_FMT(EXPANSIONINTERFACE, "EXI SD command invalid, last bit not set: {:02x}", byte);
        return;
      }

      // TODO: Check CRC

      INFO_LOG_FMT(EXPANSIONINTERFACE, "EXI SD command received: {:02x}", fmt::join(cmd.begin(), cmd.end(), " "));

      if (cmd[0] == 0x40)  // GO_IDLE_STATE
      {
        response.push_back(static_cast<u8>(R1::InIdleState));
      }
      else if (cmd[0] == 0x41)  // SEND_OP_COND
      {
        // Used by libogc for non-SDHC cards
        bool hcs = cmd[1] & 0x40;  // Host Capacity Support (for SDHC/SDXC cards)
        (void)hcs;
        response.push_back(0);  // R1 - not idle
      }
      else if (cmd[0] == 0x48)  // SEND_IF_COND
      {
        // Format R7
        u8 supply_voltage = cmd[4] & 0xF;
        u8 check_pattern = cmd[5];
        response.push_back(static_cast<u8>(R1::InIdleState));  // R1
        response.push_back(0);               // Command version nybble (0), reserved
        response.push_back(0);               // Reserved
        response.push_back(supply_voltage);  // Reserved + voltage
        response.push_back(check_pattern);
      }
      else if (cmd[0] == 0x49)  // SEND_CSD
      {
        u64 size = 0x8000000;  // TODO

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
        response.push_back(0x00);
        response.push_back(0x07);
        response.push_back(0xf0);
        response.push_back(0x03);
        response.push_back(0x5b);
        response.push_back(0x5f);
        response.push_back(0x80 | (c_size >> 10));
        response.push_back(c_size >> 2);
        response.push_back(0x3f | c_size << 6);
        response.push_back(0xfc | (c_size_mult >> 1));
        response.push_back(0x7f | (c_size << 7));
        response.push_back(0x80);
        response.push_back(0x07);
        response.push_back(0xc0);
        response.push_back(0x40);
        response.push_back(0x01 | (crc << 1));
        // Hardcoded CRC16 (0x6a74)
        response.push_back(0x6a);
        response.push_back(0x74);
      }
      else if (cmd[0] == 0x4A)  // SEND_CID
      {
        // R1
        response.push_back(0);
        // Data ready token
        response.push_back(0xfe);
        // The CID -- no idea what the format is, copied from SDIOSlot0
        response.push_back(0x80);
        response.push_back(0x11);
        response.push_back(0x4d);
        response.push_back(0x1c);
        response.push_back(0x80);
        response.push_back(0x08);
        response.push_back(0x00);
        response.push_back(0x00);
        response.push_back(0x80);
        response.push_back(0x07);
        response.push_back(0xb5);
        response.push_back(0x20);
        response.push_back(0x80);
        response.push_back(0x08);
        response.push_back(0x00);
        response.push_back(0x00);
        // Hardcoded CRC16 (0x9e3e)
        response.push_back(0x9e);
        response.push_back(0x3e);
      }
      else if (cmd[0] == 0x4c)  // STOP_TRANSMISSION
      {
        response.push_back(0);  // R1
        // There can be further padding bytes, but it's not needed
      }
      else if (cmd[0] == 0x50)  // SET_BLOCKLEN
      {
        response.push_back(0);  // R1
      }
      else if (cmd[0] == 0x77)  // APP_CMD
      {
        // The next command is an appcmd, which requires special treatment (not done here)
        response.push_back(0);  // R1
      }
      else if (cmd[0] == 0x4d)  // APP_CMD SD_STATUS
      {
        response.push_back(0);     // R1
        response.push_back(0);     // R2
        response.push_back(0xfe);  // Data ready token
        for (size_t i = 0; i < 64; i++)
        {
          response.push_back(0);
        }
        // This CRC16 is 0, probably since the data is all 0
        response.push_back(0);
        response.push_back(0);
      }
      else if (cmd[0] == 0x69)  // APP_CMD SD_SEND_OP_COND
      {
        // Used by Pokémon Channel for all cards, and libogc for SDHC cards
        bool hcs = cmd[1] & 0x40;  // Host Capacity Support (for SDHC/SDXC cards)
        (void)hcs;
        response.push_back(0);  // R1 - not idle
      }
      else
      {
        // Don't know it
        response.push_back(static_cast<u8>(R1::IllegalCommand));
      }
    }
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
