// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/Assert.h"
#include "Core/Core.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"

class CWII_IPC_HLE_Device_usb_oh1_57e_305_base : public IWII_IPC_HLE_Device
{
public:
  CWII_IPC_HLE_Device_usb_oh1_57e_305_base(u32 device_id, const std::string& device_name)
      : IWII_IPC_HLE_Device(device_id, device_name)
  {
  }
  virtual ~CWII_IPC_HLE_Device_usb_oh1_57e_305_base() override = default;

  virtual IPCCommandResult Open(u32 command_address, u32 mode) override = 0;
  virtual IPCCommandResult Close(u32 command_address, bool force) override = 0;
  IPCCommandResult IOCtl(u32 command_address) override
  {
    // NeoGamma (homebrew) is known to use this path.
    ERROR_LOG(WII_IPC_WIIMOTE, "Bad IOCtl in CWII_IPC_HLE_Device_usb_oh1_57e_305");
    Memory::Write_U32(FS_EINVAL, command_address + 4);
    return GetDefaultReply();
  }
  virtual IPCCommandResult IOCtlV(u32 command_address) override = 0;

  virtual void DoState(PointerWrap& p) override = 0;
  virtual u32 Update() override = 0;

protected:
  enum USBIOCtl
  {
    USBV0_IOCTL_CTRLMSG = 0,
    USBV0_IOCTL_BLKMSG = 1,
    USBV0_IOCTL_INTRMSG = 2,
  };

  enum USBEndpoint
  {
    HCI_CTRL = 0x00,
    HCI_EVENT = 0x81,
    ACL_DATA_IN = 0x82,
    ACL_DATA_OUT = 0x02
  };

  struct CtrlMessage
  {
    CtrlMessage() = default;
    CtrlMessage(const SIOCtlVBuffer& cmd_buffer)
    {
      request_type = Memory::Read_U8(cmd_buffer.InBuffer[0].m_Address);
      request = Memory::Read_U8(cmd_buffer.InBuffer[1].m_Address);
      value = Common::swap16(Memory::Read_U16(cmd_buffer.InBuffer[2].m_Address));
      index = Common::swap16(Memory::Read_U16(cmd_buffer.InBuffer[3].m_Address));
      length = Common::swap16(Memory::Read_U16(cmd_buffer.InBuffer[4].m_Address));
      payload_addr = cmd_buffer.PayloadBuffer[0].m_Address;
      address = cmd_buffer.m_Address;
    }

    u8 request_type;
    u8 request;
    u16 value;
    u16 index;
    u16 length;
    u32 payload_addr;
    u32 address;
  };

  class CtrlBuffer
  {
  public:
    CtrlBuffer() = default;
    CtrlBuffer(const SIOCtlVBuffer& cmd_buffer, const u32 command_address)
    {
      m_endpoint = Memory::Read_U8(cmd_buffer.InBuffer[0].m_Address);
      m_length = Memory::Read_U16(cmd_buffer.InBuffer[1].m_Address);
      m_payload_addr = cmd_buffer.PayloadBuffer[0].m_Address;
      m_cmd_address = command_address;
    }

    inline void FillBuffer(const u8* src, const size_t size) const
    {
      _dbg_assert_msg_(WII_IPC_WIIMOTE, size < m_length, "FillBuffer: size %li > payload length %i",
                       size, m_length);
      Memory::CopyToEmu(m_payload_addr, src, size);
    }
    inline void SetRetVal(const u32 retval) const { Memory::Write_U32(retval, m_cmd_address + 4); }
    inline bool IsValid() const { return m_cmd_address != 0; }
    inline void Invalidate() { m_cmd_address = m_payload_addr = 0; }
    u8 m_endpoint;
    u16 m_length;
    u32 m_payload_addr;
    u32 m_cmd_address;
  };
};

class CWII_IPC_HLE_Device_usb_oh1_57e_305_stub final
    : public CWII_IPC_HLE_Device_usb_oh1_57e_305_base
{
public:
  CWII_IPC_HLE_Device_usb_oh1_57e_305_stub(u32 device_id, const std::string& device_name)
      : CWII_IPC_HLE_Device_usb_oh1_57e_305_base(device_id, device_name)
  {
  }
  ~CWII_IPC_HLE_Device_usb_oh1_57e_305_stub() override {}
  IPCCommandResult Open(u32 command_address, u32 mode) override
  {
    PanicAlert("Bluetooth passthrough mode is enabled, but Dolphin was built without libusb."
               " Passthrough mode cannot be used.");
    return GetNoReply();
  }
  IPCCommandResult Close(u32 command_address, bool force) override { return GetNoReply(); }
  IPCCommandResult IOCtl(u32 command_address) override { return GetDefaultReply(); }
  IPCCommandResult IOCtlV(u32 command_address) override { return GetNoReply(); }
  void DoState(PointerWrap& p) override
  {
    Core::DisplayMessage("The current IPC_HLE_Device_usb is a stub. Aborting load.", 4000);
    p.SetMode(PointerWrap::MODE_VERIFY);
  }
  // Needed to be a stub for CWII_IPC_HLE_Device_usb_oh1_57e_305_real
  static void UpdateSyncButtonState() {}
  static void TriggerSyncButtonPressedEvent() {}
  static void TriggerSyncButtonHeldEvent() {}
  u32 Update() override { return 0; }
};
