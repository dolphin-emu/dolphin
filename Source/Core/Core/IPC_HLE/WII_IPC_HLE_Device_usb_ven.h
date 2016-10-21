// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#if defined(__LIBUSB__)
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Flag.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"

class PointerWrap;
struct libusb_context;
struct libusb_device;
struct libusb_device_handle;
struct libusb_transfer;

class CWII_IPC_HLE_Device_usb_ven final : public IWII_IPC_HLE_Device
{
public:
  CWII_IPC_HLE_Device_usb_ven(u32 device_id, const std::string& device_name);

  ~CWII_IPC_HLE_Device_usb_ven() override;

  IPCCommandResult Open(u32 command_address, u32 mode) override;
  IPCCommandResult Close(u32 command_address, bool force) override;
  IPCCommandResult IOCtlV(u32 command_address) override;
  IPCCommandResult IOCtl(u32 command_address) override;

  void DoState(PointerWrap& p) override;

  bool AddDevice(libusb_device* device);
  bool RemoveDevice(libusb_device* device);
  void TriggerDeviceChangeReply();

private:
  static constexpr u32 VERSION = 0x50001;

  struct TransferCommand
  {
    TransferCommand(const SIOCtlVBuffer& cmd_buffer);
    u32 cmd_address;
    s32 device_id;
  };

  struct CtrlMessage : TransferCommand
  {
    CtrlMessage(const SIOCtlVBuffer& cmd_buffer);
    u32 data_addr;
    // Setup packet fields
    u8 bmRequestType;
    u8 bmRequest;
    u16 wValue;
    u16 wIndex;
    u16 wLength;
  };

  struct BulkMessage : TransferCommand
  {
    BulkMessage(const SIOCtlVBuffer& cmd_buffer);
    u32 data_addr;
    u16 length;
    u8 endpoint;
  };

  struct IntrMessage : TransferCommand
  {
    IntrMessage(const SIOCtlVBuffer& cmd_buffer);
    u32 data_addr;
    u16 length;
    u8 endpoint;
  };

  struct IsoMessage : TransferCommand
  {
    IsoMessage(const SIOCtlVBuffer& cmd_buffer);
    u32 data_addr;
    std::vector<u16> packet_sizes;
    u8 num_packets;
    u8 endpoint;
  };

#pragma pack(push, 1)
  struct IOSDeviceEntry
  {
    s32 device_id;
    u16 vid;
    u16 pid;
    u32 token;
  };
#pragma pack(pop)

  class Device
  {
  public:
    Device(libusb_device* device);
    ~Device();
    s32 GetId() const { return m_id; }
    libusb_device* GetLibusbDevice() const { return m_device; }
    bool AttachDevice();
    int CancelTransfer(u8 endpoint);
    int SetAltSetting(u8 alt_setting);
    int SubmitTransfer(std::unique_ptr<CtrlMessage> message);
    int SubmitTransfer(std::unique_ptr<BulkMessage> message);
    int SubmitTransfer(std::unique_ptr<IntrMessage> message);
    int SubmitTransfer(std::unique_ptr<IsoMessage> message);
    std::vector<u8> GetDescriptors();

  private:
    struct PendingTransfer
    {
      std::unique_ptr<TransferCommand> command;
      libusb_transfer* transfer;
    };
    s32 m_id = 0;
    libusb_device* m_device = nullptr;
    libusb_device_handle* m_handle = nullptr;
    std::map<u8 /* endpoint */, PendingTransfer> m_pending_transfers;
    static void CtrlTransferCallback(libusb_transfer* transfer);
    static void TransferCallback(libusb_transfer* transfer);
  };

  bool m_devicechange_first_call = true;
  std::atomic<u32> m_devicechange_hook_address{0};
  std::map<s32, std::shared_ptr<Device>> m_devices;
  std::mutex m_devices_mutex;

  libusb_context* m_libusb_context = nullptr;
  int m_libusb_hotplug_handle = -1;
  // Event thread for libusb.
  Common::Flag m_thread_running;
  std::thread m_thread;
  // Device scanning thread in case libusb's hotplug support cannot be used.
  Common::Flag m_scan_thread_running;
  std::thread m_scan_thread;
  void StartThread();
  void StopThread();

  std::shared_ptr<Device> GetDeviceById(s32 device_id);
  u8 GetIOSDeviceList(std::vector<u8>* buffer);
  void UpdateDevices();
};

#else
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_stub.h"

using CWII_IPC_HLE_Device_usb_ven = CWII_IPC_HLE_Device_usb_stub;
#endif
