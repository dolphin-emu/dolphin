// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/IOS/Device.h"

namespace IOS::HLE::USB
{
constexpr u8 DEFAULT_CONFIG_NUM = 0;

enum StandardDeviceRequestCodes
{
  REQUEST_GET_DESCRIPTOR = 6,
  REQUEST_SET_CONFIGURATION = 9,
  REQUEST_GET_INTERFACE = 10,
  REQUEST_SET_INTERFACE = 11,
};

enum ControlRequestTypes
{
  DIR_HOST2DEVICE = 0,
  DIR_DEVICE2HOST = 1,
  TYPE_STANDARD = 0,
  TYPE_VENDOR = 2,
  REC_DEVICE = 0,
  REC_INTERFACE = 1,
};

constexpr u16 USBHDR(u8 dir, u8 type, u8 recipient, u8 request)
{
  return static_cast<u16>(((dir << 7 | type << 5 | recipient) << 8) | request);
}

struct DeviceDescriptor
{
  void Swap();
  u8 bLength;
  u8 bDescriptorType;
  u16 bcdUSB;
  u8 bDeviceClass;
  u8 bDeviceSubClass;
  u8 bDeviceProtocol;
  u8 bMaxPacketSize0;
  u16 idVendor;
  u16 idProduct;
  u16 bcdDevice;
  u8 iManufacturer;
  u8 iProduct;
  u8 iSerialNumber;
  u8 bNumConfigurations;
};

struct ConfigDescriptor
{
  void Swap();
  u8 bLength;
  u8 bDescriptorType;
  u16 wTotalLength;
  u8 bNumInterfaces;
  u8 bConfigurationValue;
  u8 iConfiguration;
  u8 bmAttributes;
  u8 MaxPower;
};

struct InterfaceDescriptor
{
  void Swap();
  u8 bLength;
  u8 bDescriptorType;
  u8 bInterfaceNumber;
  u8 bAlternateSetting;
  u8 bNumEndpoints;
  u8 bInterfaceClass;
  u8 bInterfaceSubClass;
  u8 bInterfaceProtocol;
  u8 iInterface;
};

struct EndpointDescriptor
{
  void Swap();
  u8 bLength;
  u8 bDescriptorType;
  u8 bEndpointAddress;
  u8 bmAttributes;
  u16 wMaxPacketSize;
  u8 bInterval;
};

struct TransferCommand
{
  Request ios_request;
  u32 data_address = 0;

  TransferCommand(EmulationKernel& ios, const Request& ios_request_, u32 data_address_)
      : ios_request(ios_request_), data_address(data_address_), m_ios(ios)
  {
  }
  virtual ~TransferCommand() = default;
  // Called after a transfer has completed to reply to the IPC request.
  // This can be overridden for additional processing before replying.
  virtual void OnTransferComplete(s32 return_value) const;
  void ScheduleTransferCompletion(s32 return_value, u32 expected_time_us) const;
  std::unique_ptr<u8[]> MakeBuffer(size_t size) const;
  void FillBuffer(const u8* src, size_t size) const;

protected:
  EmulationKernel& m_ios;
};

struct CtrlMessage : TransferCommand
{
  u8 request_type = 0;
  u8 request = 0;
  u16 value = 0;
  u16 index = 0;
  u16 length = 0;
  using TransferCommand::TransferCommand;
};

struct BulkMessage : TransferCommand
{
  u32 length = 0;
  u8 endpoint = 0;
  using TransferCommand::TransferCommand;
};

struct IntrMessage : TransferCommand
{
  u32 length = 0;
  u8 endpoint = 0;
  using TransferCommand::TransferCommand;
};

struct IsoMessage : TransferCommand
{
  u32 packet_sizes_addr = 0;
  std::vector<u16> packet_sizes;
  u32 length = 0;
  u8 num_packets = 0;
  u8 endpoint = 0;
  using TransferCommand::TransferCommand;
  void SetPacketReturnValue(size_t packet_num, u16 return_value) const;
};

class Device
{
public:
  virtual ~Device();
  u64 GetId() const;
  u16 GetVid() const;
  u16 GetPid() const;
  bool HasClass(u8 device_class) const;

  virtual DeviceDescriptor GetDeviceDescriptor() const = 0;
  virtual std::vector<ConfigDescriptor> GetConfigurations() const = 0;
  virtual std::vector<InterfaceDescriptor> GetInterfaces(u8 config) const = 0;
  virtual std::vector<EndpointDescriptor> GetEndpoints(u8 config, u8 interface, u8 alt) const = 0;

  virtual std::string GetErrorName(int error_code) const;
  /// Ensure the device is ready to use.
  virtual bool Attach() = 0;
  /// Ensure the device is ready to use and change the active interface (if needed).
  ///
  /// This may reset the active alt setting, so prefer using Attach when interface changes
  /// are unnecessary (e.g. for control requests).
  virtual bool AttachAndChangeInterface(u8 interface) = 0;
  virtual int CancelTransfer(u8 endpoint) = 0;
  virtual int ChangeInterface(u8 interface) = 0;
  virtual int GetNumberOfAltSettings(u8 interface) = 0;
  virtual int SetAltSetting(u8 alt_setting) = 0;
  virtual int SubmitTransfer(std::unique_ptr<CtrlMessage> message) = 0;
  virtual int SubmitTransfer(std::unique_ptr<BulkMessage> message) = 0;
  virtual int SubmitTransfer(std::unique_ptr<IntrMessage> message) = 0;
  virtual int SubmitTransfer(std::unique_ptr<IsoMessage> message) = 0;

protected:
  u64 m_id = 0xFFFFFFFFFFFFFFFF;
};
}  // namespace IOS::HLE::USB
