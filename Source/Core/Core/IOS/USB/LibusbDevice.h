// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#if defined(__LIBUSB__)
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/IOS/USB/Common.h"

struct libusb_config_descriptor;
struct libusb_device;
struct libusb_device_descriptor;
struct libusb_device_handle;
struct libusb_transfer;

namespace IOS
{
namespace HLE
{
namespace USB
{
// Simple wrapper around libusb_get_config_descriptor and libusb_free_config_descriptor.
class LibusbConfigDescriptor final
{
public:
  explicit LibusbConfigDescriptor(libusb_device* device, u8 config_num = 0);
  ~LibusbConfigDescriptor();
  libusb_config_descriptor* Get() const { return m_descriptor; }
  bool IsValid() const { return m_descriptor != nullptr; }
private:
  libusb_config_descriptor* m_descriptor = nullptr;
};

class LibusbDevice final : public Device
{
public:
  LibusbDevice(Kernel& ios, libusb_device* device,
               const libusb_device_descriptor& device_descriptor);
  ~LibusbDevice();
  DeviceDescriptor GetDeviceDescriptor() const override;
  std::vector<ConfigDescriptor> GetConfigurations() const override;
  std::vector<InterfaceDescriptor> GetInterfaces(u8 config) const override;
  std::vector<EndpointDescriptor> GetEndpoints(u8 config, u8 interface, u8 alt) const override;
  std::string GetErrorName(int error_code) const override;
  bool Attach(u8 interface) override;
  int CancelTransfer(u8 endpoint) override;
  int ChangeInterface(u8 interface) override;
  int GetNumberOfAltSettings(u8 interface) override;
  int SetAltSetting(u8 alt_setting) override;
  int SubmitTransfer(std::unique_ptr<CtrlMessage> message) override;
  int SubmitTransfer(std::unique_ptr<BulkMessage> message) override;
  int SubmitTransfer(std::unique_ptr<IntrMessage> message) override;
  int SubmitTransfer(std::unique_ptr<IsoMessage> message) override;

private:
  Kernel& m_ios;

  std::vector<std::unique_ptr<LibusbConfigDescriptor>> m_config_descriptors;
  u16 m_vid = 0;
  u16 m_pid = 0;
  u8 m_active_interface = 0;
  bool m_device_attached = false;

  libusb_device* m_device = nullptr;
  libusb_device_handle* m_handle = nullptr;

  class TransferEndpoint final
  {
  public:
    void AddTransfer(std::unique_ptr<TransferCommand> command, libusb_transfer* transfer);
    void HandleTransfer(libusb_transfer* tr, std::function<s32(const TransferCommand&)> function);
    void CancelTransfers();

  private:
    std::mutex m_transfers_mutex;
    std::map<libusb_transfer*, std::unique_ptr<TransferCommand>> m_transfers;
  };
  std::map<u8, TransferEndpoint> m_transfer_endpoints;
  static void CtrlTransferCallback(libusb_transfer* transfer);
  static void TransferCallback(libusb_transfer* transfer);

  int AttachInterface(u8 interface);
  int DetachInterface();
};
}  // namespace USB
}  // namespace HLE
}  // namespace IOS
#endif
