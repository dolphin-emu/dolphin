// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/SDIO/SDIOSlot0.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/SDCardUtil.h"
#include "Core/ConfigManager.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/IOS.h"

namespace IOS
{
namespace HLE
{
namespace Device
{
inline bool SupportsSDHC(u32 ios_version)
{
  switch (ios_version)
  {
  // Known versions to support SDHC
  case 48:
  case 56:
  case 57:
  case 58:
  case 59:
  case 60:
  case 61:
  case 70:
  case 80:
    return true;
  default:
    return false;
  };
}

SDIOSlot0::SDIOSlot0(Kernel& ios, const std::string& device_name)
    : Device(ios, device_name), m_sdhc_supported(SupportsSDHC(ios.GetVersion()))
{
}

void SDIOSlot0::DoState(PointerWrap& p)
{
  DoStateShared(p);
  if (p.GetMode() == PointerWrap::MODE_READ)
  {
    OpenInternal();
  }
  p.Do(m_status);
  p.Do(m_block_length);
  p.Do(m_bus_width);
  p.Do(m_registers);
  p.Do(m_protocol);
  p.Do(m_sdhc_supported);
}

void SDIOSlot0::EventNotify()
{
  if (!m_event)
    return;
  // Accessing SConfig variables like this isn't really threadsafe,
  // but this is how it's done all over the place...
  if ((SConfig::GetInstance().m_WiiSDCard && m_event->type == EVENT_INSERT) ||
      (!SConfig::GetInstance().m_WiiSDCard && m_event->type == EVENT_REMOVE))
  {
    m_ios.EnqueueIPCReply(m_event->request, m_event->type);
    m_event.reset();
  }
}

void SDIOSlot0::OpenInternal()
{
  const std::string filename = File::GetUserPath(F_WIISDCARD_IDX);
  m_card.Open(filename, "r+b");
  if (!m_card)
  {
    WARN_LOG(IOS_SD, "Failed to open SD Card image, trying to create a new 128MB image...");
    if (SDCardCreate(128, filename))
    {
      INFO_LOG(IOS_SD, "Successfully created %s", filename.c_str());
      m_card.Open(filename, "r+b");
    }
    if (!m_card)
    {
      ERROR_LOG(IOS_SD, "Could not open SD Card image or create a new one, are you running "
                        "from a read-only directory?");
    }
  }
}

ReturnCode SDIOSlot0::Open(const OpenRequest& request)
{
  OpenInternal();
  m_registers.fill(0);

  m_is_active = true;

  return IPC_SUCCESS;
}

ReturnCode SDIOSlot0::Close(u32 fd)
{
  m_card.Close();
  m_block_length = 0;
  m_bus_width = 0;

  return Device::Close(fd);
}

IPCCommandResult SDIOSlot0::IOCtl(const IOCtlRequest& request)
{
  Memory::Memset(request.buffer_out, 0, request.buffer_out_size);

  switch (request.request)
  {
  case IOCTL_WRITEHCR:
    return WriteHCRegister(request);
  case IOCTL_READHCR:
    return ReadHCRegister(request);
  case IOCTL_RESETCARD:
    return ResetCard(request);
  case IOCTL_SETCLK:
    return SetClk(request);
  case IOCTL_SENDCMD:
    return SendCommand(request);
  case IOCTL_GETSTATUS:
    return GetStatus(request);
  case IOCTL_GETOCR:
    return GetOCRegister(request);
  default:
    ERROR_LOG(IOS_SD, "Unknown SD IOCtl command (0x%08x)", request.request);
    break;
  }

  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult SDIOSlot0::IOCtlV(const IOCtlVRequest& request)
{
  switch (request.request)
  {
  case IOCTLV_SENDCMD:
    return SendCommand(request);
  default:
    ERROR_LOG(IOS_SD, "Unknown SD IOCtlV command 0x%08x", request.request);
    break;
  }

  return GetDefaultReply(IPC_SUCCESS);
}

s32 SDIOSlot0::ExecuteCommand(const Request& request, u32 _BufferIn, u32 _BufferInSize,
                              u32 _rwBuffer, u32 _rwBufferSize, u32 _BufferOut, u32 _BufferOutSize)
{
  // The game will send us a SendCMD with this information. To be able to read and write
  // to a file we need to prepare a 0x10 byte output buffer as response.
  struct Request
  {
    u32 command;
    u32 type;
    u32 resp;
    u32 arg;
    u32 blocks;
    u32 bsize;
    u32 addr;
    u32 isDMA;
    u32 pad0;
  } req;

  req.command = Memory::Read_U32(_BufferIn + 0);
  req.type = Memory::Read_U32(_BufferIn + 4);
  req.resp = Memory::Read_U32(_BufferIn + 8);
  req.arg = Memory::Read_U32(_BufferIn + 12);
  req.blocks = Memory::Read_U32(_BufferIn + 16);
  req.bsize = Memory::Read_U32(_BufferIn + 20);
  req.addr = Memory::Read_U32(_BufferIn + 24);
  req.isDMA = Memory::Read_U32(_BufferIn + 28);
  req.pad0 = Memory::Read_U32(_BufferIn + 32);

  // Note: req.addr is the virtual address of _rwBuffer

  s32 ret = RET_OK;

  switch (req.command)
  {
  case GO_IDLE_STATE:
    INFO_LOG(IOS_SD, "GO_IDLE_STATE");
    // Response is R1 (idle state)
    Memory::Write_U32(0x00, _BufferOut);
    break;

  case SEND_RELATIVE_ADDR:
    // Technically RCA should be generated when asked and at power on...w/e :p
    Memory::Write_U32(0x9f62, _BufferOut);
    break;

  case SELECT_CARD:
    // This covers both select and deselect
    // Differentiate by checking if rca is set in req.arg
    // If it is, it's a select and return 0x700
    Memory::Write_U32((req.arg >> 16) ? 0x700 : 0x900, _BufferOut);
    break;

  case SEND_IF_COND:
    INFO_LOG(IOS_SD, "SEND_IF_COND");
    // If the card can operate on the supplied voltage, the response echoes back the supply
    // voltage and the check pattern that were set in the command argument.
    // This command is used to differentiate between protocol v1 and v2.
    InitSDHC();
    Memory::Write_U32(req.arg, _BufferOut);
    break;

  case SEND_CSD:
  {
    const std::array<u32, 4> csd = m_protocol == SDProtocol::V1 ? GetCSDv1() : GetCSDv2();
    Memory::CopyToEmuSwapped(_BufferOut, csd.data(), csd.size() * sizeof(u32));
  }
  break;

  case ALL_SEND_CID:
  case SEND_CID:
    INFO_LOG(IOS_SD, "(ALL_)SEND_CID");
    Memory::Write_U32(0x80114d1c, _BufferOut);
    Memory::Write_U32(0x80080000, _BufferOut + 4);
    Memory::Write_U32(0x8007b520, _BufferOut + 8);
    Memory::Write_U32(0x80080000, _BufferOut + 12);
    break;

  case SET_BLOCKLEN:
    m_block_length = req.arg;
    Memory::Write_U32(0x900, _BufferOut);
    break;

  case APP_CMD_NEXT:
    // Next cmd is going to be ACMD_*
    Memory::Write_U32(0x920, _BufferOut);
    break;

  case ACMD_SETBUSWIDTH:
    // 0 = 1bit, 2 = 4bit
    m_bus_width = (req.arg & 3);
    Memory::Write_U32(0x920, _BufferOut);
    break;

  case ACMD_SENDOPCOND:
    // Sends host capacity support information (HCS) and asks the accessed card to send
    // its operating condition register (OCR) content
    Memory::Write_U32(GetOCRegister(), _BufferOut);
    break;

  case READ_MULTIPLE_BLOCK:
  {
    // Data address (req.arg) is in byte units in a Standard Capacity SD Memory Card
    // and in block (512 Byte) units in a High Capacity SD Memory Card.
    INFO_LOG(IOS_SD, "%sRead %i Block(s) from 0x%08x bsize %i into 0x%08x!",
             req.isDMA ? "DMA " : "", req.blocks, req.arg, req.bsize, req.addr);

    if (m_card)
    {
      u32 size = req.bsize * req.blocks;
      u64 address = GetAddressFromRequest(req.arg);

      if (!m_card.Seek(address, SEEK_SET))
        ERROR_LOG(IOS_SD, "Seek failed WTF");

      if (m_card.ReadBytes(Memory::GetPointer(req.addr), size))
      {
        DEBUG_LOG(IOS_SD, "Outbuffer size %i got %i", _rwBufferSize, size);
      }
      else
      {
        ERROR_LOG(IOS_SD, "Read Failed - error: %i, eof: %i", ferror(m_card.GetHandle()),
                  feof(m_card.GetHandle()));
        ret = RET_FAIL;
      }
    }
  }
    Memory::Write_U32(0x900, _BufferOut);
    break;

  case WRITE_MULTIPLE_BLOCK:
  {
    // Data address (req.arg) is in byte units in a Standard Capacity SD Memory Card
    // and in block (512 Byte) units in a High Capacity SD Memory Card.
    INFO_LOG(IOS_SD, "%sWrite %i Block(s) from 0x%08x bsize %i to offset 0x%08x!",
             req.isDMA ? "DMA " : "", req.blocks, req.addr, req.bsize, req.arg);

    if (m_card && SConfig::GetInstance().bEnableMemcardSdWriting)
    {
      u32 size = req.bsize * req.blocks;
      u64 address = GetAddressFromRequest(req.arg);

      if (!m_card.Seek(address, SEEK_SET))
        ERROR_LOG(IOS_SD, "fseeko failed WTF");

      if (!m_card.WriteBytes(Memory::GetPointer(req.addr), size))
      {
        ERROR_LOG(IOS_SD, "Write Failed - error: %i, eof: %i", ferror(m_card.GetHandle()),
                  feof(m_card.GetHandle()));
        ret = RET_FAIL;
      }
    }
  }
    Memory::Write_U32(0x900, _BufferOut);
    break;

  case EVENT_REGISTER:  // async
    INFO_LOG(IOS_SD, "Register event %x", req.arg);
    m_event = std::make_unique<Event>(static_cast<EventType>(req.arg), request);
    ret = RET_EVENT_REGISTER;
    break;

  // Used to cancel an event that was already registered.
  case EVENT_UNREGISTER:
  {
    INFO_LOG(IOS_SD, "Unregister event %x", req.arg);
    if (!m_event)
      return IPC_EINVAL;
    // release returns 0
    // unknown sd int
    // technically we do it out of order, oh well
    m_ios.EnqueueIPCReply(m_event->request, EVENT_INVALID);
    m_event.reset();
    break;
  }

  default:
    ERROR_LOG(IOS_SD, "Unknown SD command 0x%08x", req.command);
    break;
  }

  return ret;
}

IPCCommandResult SDIOSlot0::WriteHCRegister(const IOCtlRequest& request)
{
  u32 reg = Memory::Read_U32(request.buffer_in);
  u32 val = Memory::Read_U32(request.buffer_in + 16);

  INFO_LOG(IOS_SD, "IOCTL_WRITEHCR 0x%08x - 0x%08x", reg, val);

  if (reg >= m_registers.size())
  {
    WARN_LOG(IOS_SD, "IOCTL_WRITEHCR out of range");
    return GetDefaultReply(IPC_SUCCESS);
  }

  if ((reg == HCR_CLOCKCONTROL) && (val & 1))
  {
    // Clock is set to oscillate, enable bit 1 to say it's stable
    m_registers[reg] = val | 2;
  }
  else if ((reg == HCR_SOFTWARERESET) && val)
  {
    // When a reset is specified, the register gets cleared
    m_registers[reg] = 0;
  }
  else
  {
    // Default to just storing the new value
    m_registers[reg] = val;
  }

  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult SDIOSlot0::ReadHCRegister(const IOCtlRequest& request)
{
  u32 reg = Memory::Read_U32(request.buffer_in);

  if (reg >= m_registers.size())
  {
    WARN_LOG(IOS_SD, "IOCTL_READHCR out of range");
    return GetDefaultReply(IPC_SUCCESS);
  }

  u32 val = m_registers[reg];
  INFO_LOG(IOS_SD, "IOCTL_READHCR 0x%08x - 0x%08x", reg, val);

  // Just reading the register
  Memory::Write_U32(val, request.buffer_out);
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult SDIOSlot0::ResetCard(const IOCtlRequest& request)
{
  INFO_LOG(IOS_SD, "IOCTL_RESETCARD");

  // Returns 16bit RCA and 16bit 0s (meaning success)
  Memory::Write_U32(m_status, request.buffer_out);

  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult SDIOSlot0::SetClk(const IOCtlRequest& request)
{
  INFO_LOG(IOS_SD, "IOCTL_SETCLK");

  // libogc only sets it to 1 and makes sure the return isn't negative...
  // one half of the sdclk divisor: a power of two or zero.
  u32 clock = Memory::Read_U32(request.buffer_in);
  if (clock != 1)
    INFO_LOG(IOS_SD, "Setting to %i, interesting", clock);

  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult SDIOSlot0::SendCommand(const IOCtlRequest& request)
{
  INFO_LOG(IOS_SD, "IOCTL_SENDCMD %x IPC:%08x", Memory::Read_U32(request.buffer_in),
           request.address);

  const s32 return_value = ExecuteCommand(request, request.buffer_in, request.buffer_in_size, 0, 0,
                                          request.buffer_out, request.buffer_out_size);

  if (return_value == RET_EVENT_REGISTER)
  {
    // Check if the condition is already true
    EventNotify();
    return GetNoReply();
  }

  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult SDIOSlot0::GetStatus(const IOCtlRequest& request)
{
  // Since IOS does the SD initialization itself, we just say we're always initialized.
  if (m_card)
  {
    if (m_card.GetSize() <= SDSC_MAX_SIZE)
    {
      // No further initialization required.
      m_status |= CARD_INITIALIZED;
    }
    else
    {
      // Some IOS versions support SDHC.
      // Others will work if they are manually initialized (SEND_IF_COND)
      if (m_sdhc_supported)
      {
        // All of the initialization is done internally by IOS, so we get to skip some steps.
        InitSDHC();
      }
      m_status |= CARD_SDHC;
    }
  }

  // Evaluate whether a card is currently inserted (config value).
  // Make sure we don't modify m_status so we don't lose track of whether the card is SDHC.
  const u32 status =
      SConfig::GetInstance().m_WiiSDCard ? (m_status | CARD_INSERTED) : CARD_NOT_EXIST;

  INFO_LOG(IOS_SD, "IOCTL_GETSTATUS. Replying that %s card is %s%s",
           (status & CARD_SDHC) ? "SDHC" : "SD",
           (status & CARD_INSERTED) ? "inserted" : "not present",
           (status & CARD_INITIALIZED) ? " and initialized" : "");

  Memory::Write_U32(status, request.buffer_out);
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult SDIOSlot0::GetOCRegister(const IOCtlRequest& request)
{
  u32 ocr = GetOCRegister();
  INFO_LOG(IOS_SD, "IOCTL_GETOCR. Replying with ocr %x", ocr);
  Memory::Write_U32(ocr, request.buffer_out);

  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult SDIOSlot0::SendCommand(const IOCtlVRequest& request)
{
  DEBUG_LOG(IOS_SD, "IOCTLV_SENDCMD 0x%08x", Memory::Read_U32(request.in_vectors[0].address));
  Memory::Memset(request.io_vectors[0].address, 0, request.io_vectors[0].size);

  const s32 return_value =
      ExecuteCommand(request, request.in_vectors[0].address, request.in_vectors[0].size,
                     request.in_vectors[1].address, request.in_vectors[1].size,
                     request.io_vectors[0].address, request.io_vectors[0].size);

  return GetDefaultReply(return_value);
}

u32 SDIOSlot0::GetOCRegister() const
{
  u32 ocr = 0x00ff8000;
  if (m_status & CARD_INITIALIZED)
    ocr |= 0x80000000;
  if (m_status & CARD_SDHC)
    ocr |= 0x40000000;
  return ocr;
}

std::array<u32, 4> SDIOSlot0::GetCSDv1() const
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
      ERROR_LOG(IOS_SD, "SD Card is too big!");
      // Set max values
      size = 4096;
      c_size_mult = 7 + 2 + read_bl_len;
    }
  }
  c_size_mult -= 2 + read_bl_len;
  --size;
  const u32 c_size(size);

  if (invalid_size)
    WARN_LOG(IOS_SD, "SD Card size is invalid");
  else
    INFO_LOG(IOS_SD, "SD C_SIZE = %u, C_SIZE_MULT = %u", c_size, c_size_mult);

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
  return {{
      0x007f003, 0x5b5f8000 | (c_size >> 2), 0x3ffc7f80 | (c_size << 30) | (c_size_mult << 15),
      0x07c04001 | (crc << 1),
  }};
}

std::array<u32, 4> SDIOSlot0::GetCSDv2() const
{
  const u64 size = m_card.GetSize();

  if (size % (512 * 1024) != 0)
    WARN_LOG(IOS_SD, "SDHC Card size cannot be divided by 1024 * 512");

  const u32 c_size(size / (512 * 1024) - 1);

  // 0b01               CSD_STRUCTURE (SDv2)
  // 0b000000           reserved
  // 0b00001110         TAAC (1.0 * 1ms)
  // 0b00000000         NSAC
  // 0b01011010         TRAN_SPEED (5.0 * 10 Mbit/s, max operating frequency)

  // 0b010111110101     CCC (TODO: Figure out what each command class does)
  // 0b1001             READ_BL_LEN (512 bytes, fixed for SDHC)
  // 0b0                READ_BL_PARTIAL
  // 0b0                WRITE_BLK_MISALIGN
  // 0b0                READ_BLK_MISALIGN
  // 0b0                DSR_IMP (no driver stage register implemented)
  // 0b000000           reserved
  // 0b??????           C_SIZE (most significant 6 bits)

  // 0b???????????????? C_SIZE (least significant 16 bits)
  // 0b0                reserved
  // 0b1                ERASE_BLK_EN
  // 0b1111111          SECTOR_SIZE
  // 0b0000000          WP_GRP_SIZE (not supported in SDHC)

  // 0b0                WP_GRP_ENABLE
  // 0b00               reserved
  // 0b010              R2W_FACTOR (x4)
  // 0b1001             WRITE_BL_LEN (512 bytes)
  // 0b0                WRITE_BL_PARTIAL
  // 0b00000            reserved
  // 0b0                FILE_FORMAT_GRP
  // 0b0                COPY
  // 0b0                PERM_WRITE_PROTECT
  // 0b0                TMP_WRITE_PROTECT
  // 0b00               FILE_FORMAT
  // 0b00               reserved
  // 0b0000000          CRC
  // 0b1                reserved

  // TODO: CRC7 (but so far it looks like nobody is actually verifying this)
  constexpr u32 crc = 0;

  // Form the csd using the description above
  return {{
      0x400e005a, 0x5f590000 | (c_size >> 16), 0x00007f80 | (c_size << 16), 0x0a400001 | (crc << 1),
  }};
}

u64 SDIOSlot0::GetAddressFromRequest(u32 arg) const
{
  u64 address(arg);
  if (m_status & CARD_SDHC)
    address *= 512;
  return address;
}

void SDIOSlot0::InitSDHC()
{
  m_protocol = SDProtocol::V2;
  m_status |= CARD_INITIALIZED;
}

}  // namespace Device
}  // namespace HLE
}  // namespace IOS
