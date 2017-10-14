// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// PRELIMINARY - seems to fully work with libogc, writing has yet to be tested

#pragma once

#include <array>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/IOS.h"

class PointerWrap;

namespace IOS
{
namespace HLE
{
namespace Device
{
// The front SD slot
class SDIOSlot0 : public Device
{
public:
  SDIOSlot0(Kernel& ios, const std::string& device_name);

  void DoState(PointerWrap& p) override;

  ReturnCode Open(const OpenRequest& request) override;
  ReturnCode Close(u32 fd) override;
  IPCCommandResult IOCtl(const IOCtlRequest& request) override;
  IPCCommandResult IOCtlV(const IOCtlVRequest& request) override;

  void EventNotify();

private:
  // SD Host Controller Registers
  enum
  {
    HCR_CLOCKCONTROL = 0x2C,
    HCR_SOFTWARERESET = 0x2F,
  };

  // IOCtl
  enum
  {
    IOCTL_WRITEHCR = 0x01,
    IOCTL_READHCR = 0x02,
    IOCTL_RESETCARD = 0x04,
    IOCTL_SETCLK = 0x06,
    IOCTL_SENDCMD = 0x07,
    IOCTL_GETSTATUS = 0x0B,
    IOCTL_GETOCR = 0x0C,
  };

  // IOCtlV
  enum
  {
    IOCTLV_SENDCMD = 0x07,
  };

  // ExecuteCommand
  enum
  {
    RET_OK,
    RET_FAIL,
    RET_EVENT_REGISTER,  // internal state only - not actually returned
  };

  // Status
  enum
  {
    CARD_NOT_EXIST = 0,
    CARD_INSERTED = 1,
    CARD_INITIALIZED = 0x10000,
    CARD_SDHC = 0x100000,
  };

  // Commands
  enum
  {
    GO_IDLE_STATE = 0x00,
    ALL_SEND_CID = 0x02,
    SEND_RELATIVE_ADDR = 0x03,
    SELECT_CARD = 0x07,
    SEND_IF_COND = 0x08,
    SEND_CSD = 0x09,
    SEND_CID = 0x0A,
    SEND_STATUS = 0x0D,
    SET_BLOCKLEN = 0x10,
    READ_MULTIPLE_BLOCK = 0x12,
    WRITE_MULTIPLE_BLOCK = 0x19,
    APP_CMD_NEXT = 0x37,

    ACMD_SETBUSWIDTH = 0x06,
    ACMD_SENDOPCOND = 0x29,
    ACMD_SENDSCR = 0x33,

    EVENT_REGISTER = 0x40,
    EVENT_UNREGISTER = 0x41,
  };

  enum EventType
  {
    EVENT_INSERT = 1,
    EVENT_REMOVE = 2,
    // from unregister, i think it is just meant to be invalid
    EVENT_INVALID = 0xc210000
  };

  enum class SDProtocol
  {
    V1,
    V2,
  };

  // Number of bytes to trigger using SDHC instead of SDSC
  static constexpr u32 SDHC_BYTES = 0x80000000;

  struct Event
  {
    Event(EventType type_, Request request_) : type(type_), request(request_) {}
    EventType type;
    Request request;
  };

  IPCCommandResult WriteHCRegister(const IOCtlRequest& request);
  IPCCommandResult ReadHCRegister(const IOCtlRequest& request);
  IPCCommandResult ResetCard(const IOCtlRequest& request);
  IPCCommandResult SetClk(const IOCtlRequest& request);
  IPCCommandResult SendCommand(const IOCtlRequest& request);
  IPCCommandResult GetStatus(const IOCtlRequest& request);
  IPCCommandResult GetOCRegister(const IOCtlRequest& request);

  IPCCommandResult SendCommand(const IOCtlVRequest& request);

  s32 ExecuteCommand(const Request& request, u32 BufferIn, u32 BufferInSize, u32 BufferIn2,
                     u32 BufferInSize2, u32 _BufferOut, u32 BufferOutSize);
  void OpenInternal();
  void InitStatus();

  u32 GetOCRegister() const;

  std::array<u32, 4> GetCSDv1() const;
  std::array<u32, 4> GetCSDv2() const;
  void InitSDHC();

  u64 GetAddressFromRequest(u32 arg) const;

  // TODO: do we need more than one?
  std::unique_ptr<Event> m_event;

  u32 m_status = CARD_NOT_EXIST;
  SDProtocol m_protocol = SDProtocol::V1;

  // Is SDHC supported by the IOS?
  // Other IOS requires manual SDHC initialization
  const bool m_sdhc_supported;

  u32 m_block_length = 0;
  u32 m_bus_width = 0;

  std::array<u32, 0x200 / sizeof(u32)> m_registers;

  File::IOFile m_card;
};
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
