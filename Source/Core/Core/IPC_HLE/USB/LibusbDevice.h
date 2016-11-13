// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#if defined(__LIBUSB__)
#include <cstddef>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/IPC_HLE/USB/Common.h"

struct libusb_config_descriptor;
struct libusb_device;
struct libusb_device_handle;
struct libusb_transfer;

class LibusbDevice final : public USBDevice
{
public:
  LibusbDevice(libusb_device* device, u8 interface);
  ~LibusbDevice();
  std::string GetErrorName(int error_code) const override;
  bool AttachDevice() override;
  int CancelTransfer(u8 endpoint) override;
  int ChangeInterface(u8 interface) override;
  int SetAltSetting(u8 alt_setting) override;
  int SubmitTransfer(std::unique_ptr<CtrlMessage> message) override;
  int SubmitTransfer(std::unique_ptr<BulkMessage> message) override;
  int SubmitTransfer(std::unique_ptr<IntrMessage> message) override;
  int SubmitTransfer(std::unique_ptr<IsoMessage> message) override;
  std::vector<u8> GetIOSDescriptors(IOSVersion, u8 alt_setting) override;

private:
  class TransferEndpoint final
  {
  public:
    // Cancel all transfers before destructing the device.
    ~TransferEndpoint();
    void AddTransfer(std::unique_ptr<TransferCommand> command, libusb_transfer* transfer);
    void HandleTransfer(libusb_transfer* tr, std::function<void(const TransferCommand&)> function);
    void CancelTransfers();

  private:
    std::mutex m_transfers_mutex;
    std::map<libusb_transfer*, std::unique_ptr<TransferCommand>> m_transfers;
  };

  bool m_device_attached = false;
  u8 m_num_interfaces = 1;
  libusb_device* m_device = nullptr;
  libusb_device_handle* m_handle = nullptr;

  std::map<u8, TransferEndpoint> m_transfer_endpoints;
  static void CtrlTransferCallback(libusb_transfer* transfer);
  static void TransferCallback(libusb_transfer* transfer);

  int AttachInterface(u8 interface);
  int DetachInterface(u8 interface);
};

// Simple wrapper around libusb_get_config_descriptor and libusb_free_config_descriptor.
class LibusbConfigDescriptor final
{
public:
  explicit LibusbConfigDescriptor(libusb_device* device, u8 config_num = 0);
  ~LibusbConfigDescriptor();
  bool IsValid() const { return m_descriptor != nullptr; }
  libusb_config_descriptor* m_descriptor = nullptr;
};

#else
#include "Core/IPC_HLE/USB/StubDevice.h"

using LibusbDevice = StubDevice;
#endif
