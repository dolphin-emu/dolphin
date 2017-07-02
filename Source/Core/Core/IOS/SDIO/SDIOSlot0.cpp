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
SDIOSlot0::SDIOSlot0(Kernel& ios, const std::string& device_name) : Device(ios, device_name)
{
}

void SDIOSlot0::DoState(PointerWrap& p)
{
  DoStateShared(p);
  if (p.GetMode() == PointerWrap::MODE_READ)
  {
    OpenInternal();
  }
  p.Do(m_Status);
  p.Do(m_BlockLength);
  p.Do(m_BusWidth);
  p.Do(m_registers);
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
  m_Card.Open(filename, "r+b");
  if (!m_Card)
  {
    WARN_LOG(IOS_SD, "Failed to open SD Card image, trying to create a new 128MB image...");
    if (SDCardCreate(128, filename))
    {
      INFO_LOG(IOS_SD, "Successfully created %s", filename.c_str());
      m_Card.Open(filename, "r+b");
    }
    if (!m_Card)
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
  m_Card.Close();
  m_BlockLength = 0;
  m_BusWidth = 0;

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
    // libogc can use it during init..
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
    // If the card can operate on the supplied voltage, the response echoes back the supply
    // voltage and the check pattern that were set in the command argument.
    Memory::Write_U32(req.arg, _BufferOut);
    break;

  case SEND_CSD:
    INFO_LOG(IOS_SD, "SEND_CSD");
    // <WntrMute> shuffle2_, OCR: 0x80ff8000 CID: 0x38a00000 0x480032d5 0x3c608030 0x8803d420
    // CSD: 0xff928040 0xc93efbcf 0x325f5a83 0x00002600

    // Values used currently are from lpfaint99
    Memory::Write_U32(0x80168000, _BufferOut);
    Memory::Write_U32(0xa9ffffff, _BufferOut + 4);
    Memory::Write_U32(0x325b5a83, _BufferOut + 8);
    Memory::Write_U32(0x00002e00, _BufferOut + 12);
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
    m_BlockLength = req.arg;
    Memory::Write_U32(0x900, _BufferOut);
    break;

  case APP_CMD_NEXT:
    // Next cmd is going to be ACMD_*
    Memory::Write_U32(0x920, _BufferOut);
    break;

  case ACMD_SETBUSWIDTH:
    // 0 = 1bit, 2 = 4bit
    m_BusWidth = (req.arg & 3);
    Memory::Write_U32(0x920, _BufferOut);
    break;

  case ACMD_SENDOPCOND:
    // Sends host capacity support information (HCS) and asks the accessed card to send
    // its operating condition register (OCR) content
    Memory::Write_U32(0x80ff8000, _BufferOut);
    break;

  case READ_MULTIPLE_BLOCK:
  {
    // Data address (req.arg) is in byte units in a Standard Capacity SD Memory Card
    // and in block (512 Byte) units in a High Capacity SD Memory Card.
    INFO_LOG(IOS_SD, "%sRead %i Block(s) from 0x%08x bsize %i into 0x%08x!",
             req.isDMA ? "DMA " : "", req.blocks, req.arg, req.bsize, req.addr);

    if (m_Card)
    {
      u32 size = req.bsize * req.blocks;

      if (!m_Card.Seek(req.arg, SEEK_SET))
        ERROR_LOG(IOS_SD, "Seek failed WTF");

      if (m_Card.ReadBytes(Memory::GetPointer(req.addr), size))
      {
        DEBUG_LOG(IOS_SD, "Outbuffer size %i got %i", _rwBufferSize, size);
      }
      else
      {
        ERROR_LOG(IOS_SD, "Read Failed - error: %i, eof: %i", ferror(m_Card.GetHandle()),
                  feof(m_Card.GetHandle()));
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

    if (m_Card && SConfig::GetInstance().bEnableMemcardSdWriting)
    {
      u32 size = req.bsize * req.blocks;

      if (!m_Card.Seek(req.arg, SEEK_SET))
        ERROR_LOG(IOS_SD, "fseeko failed WTF");

      if (!m_Card.WriteBytes(Memory::GetPointer(req.addr), size))
      {
        ERROR_LOG(IOS_SD, "Write Failed - error: %i, eof: %i", ferror(m_Card.GetHandle()),
                  feof(m_Card.GetHandle()));
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

  if (m_Card)
    m_Status |= CARD_INITIALIZED;

  // Returns 16bit RCA and 16bit 0s (meaning success)
  Memory::Write_U32(0x9f620000, request.buffer_out);
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
  if (SConfig::GetInstance().m_WiiSDCard)
    m_Status |= CARD_INSERTED;
  else
    m_Status = CARD_NOT_EXIST;

  INFO_LOG(IOS_SD, "IOCTL_GETSTATUS. Replying that SD card is %s%s",
           (m_Status & CARD_INSERTED) ? "inserted" : "not present",
           (m_Status & CARD_INITIALIZED) ? " and initialized" : "");

  Memory::Write_U32(m_Status, request.buffer_out);
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult SDIOSlot0::GetOCRegister(const IOCtlRequest& request)
{
  INFO_LOG(IOS_SD, "IOCTL_GETOCR");
  Memory::Write_U32(0x80ff8000, request.buffer_out);
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
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
