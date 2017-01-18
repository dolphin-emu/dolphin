// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/IOS/Device.h"

namespace IOS
{
namespace HLE
{
namespace USB
{
struct DeviceDescriptor
{
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

  TransferCommand(const Request& ios_request_, u32 data_address_)
      : ios_request(ios_request_), data_address(data_address_)
  {
  }
  virtual ~TransferCommand() = default;
  // Called after a transfer has completed and before replying to the transfer request.
  virtual void OnTransferComplete() const {}
  std::unique_ptr<u8[]> MakeBuffer(size_t size) const;
  void FillBuffer(const u8* src, size_t size) const;
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
  u16 length = 0;
  u8 endpoint = 0;
  using TransferCommand::TransferCommand;
};

struct IntrMessage : TransferCommand
{
  u16 length = 0;
  u8 endpoint = 0;
  using TransferCommand::TransferCommand;
};
}  // namespace USB
}  // namespace HLE
}  // namespace IOS
