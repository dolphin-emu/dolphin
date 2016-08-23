// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <thread>

#include <libusb.h>

#include "Common/Assert.h"
#include "Common/Flag.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"

class CWII_IPC_HLE_Device_usb_oh1_57e_305 : public IWII_IPC_HLE_Device
{
public:
  CWII_IPC_HLE_Device_usb_oh1_57e_305(u32 device_id, const std::string& device_name);
  ~CWII_IPC_HLE_Device_usb_oh1_57e_305() override;

  IPCCommandResult Open(u32 command_address, u32 mode) override;
  IPCCommandResult Close(u32 command_address, bool force) override;
  IPCCommandResult IOCtl(u32 command_address) override;
  IPCCommandResult IOCtlV(u32 command_address) override;

  void DoState(PointerWrap& p) override;
  u32 Update() override { return 0; }
  static void TriggerSyncButtonEvent();

private:
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
    u8 m_endpoint;
    u16 m_length;
    u32 m_payload_addr;
    u32 m_cmd_address;
  };

  struct HCIEventCommand
  {
    u8 event_type;
    u8 payload_length;
    u8 packet_indicator;
    u16 opcode;
  };

  static constexpr u8 INTERFACE = 0x00;
  static constexpr u8 PROTOCOL_BLUETOOTH = 0x01;
  // Arbitrarily chosen value that allows emulated software to send commands often enough
  // so that the sync button event is triggered at least every 200ms.
  // Ideally this should be equal to 0, so we don't trigger unnecessary libusb transfers.
  static constexpr int TIMEOUT = 200;

  libusb_device* m_device = nullptr;
  libusb_device_handle* m_handle = nullptr;
  libusb_context* m_libusb_context = nullptr;

  Common::Flag m_thread_running;
  std::thread m_thread;

  void SendHCICommandPacket(const CtrlMessage& cmd_message);
  void SendHCIResetCommand();

  void FakeEventCommandComplete(u16 opcode, const void* data, u32 size, const CtrlMessage& cmd);
  static void FakeSyncButtonEvent(const CtrlBuffer& ctrl);

  bool OpenDevice(libusb_device* device);
  void StartThread();
  void StopThread();
  void ThreadFunc();
  static void TransferCallback(libusb_transfer* transfer);
};
