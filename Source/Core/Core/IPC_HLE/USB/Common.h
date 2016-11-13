// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

enum StandardDeviceRequestCodes
{
  REQUEST_SET_CONFIGURATION = 9,
  REQUEST_GET_INTERFACE = 10,
  REQUEST_SET_INTERFACE = 11,
};

enum ControlRequestTypes
{
  USB_DIR_HOST2DEVICE = 0,
  USB_DIR_DEVICE2HOST = 1,
  USB_TYPE_STANDARD = 0,
  USB_TYPE_VENDOR = 2,
  USB_REC_DEVICE = 0,
  USB_REC_INTERFACE = 1,
};

struct IOSDeviceDescriptor
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
  u8 pad[2] = {0};
};

struct IOSConfigDescriptor
{
  u8 bLength;
  u8 bDescriptorType;
  u16 wTotalLength;
  u8 bNumInterfaces;
  u8 bConfigurationValue;
  u8 iConfiguration;
  u8 bmAttributes;
  u8 MaxPower;
  u8 pad[3];
};

struct IOSInterfaceDescriptor
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
  u8 pad[3];
};

struct IOSEndpointDescriptor
{
  u8 bLength;
  u8 bDescriptorType;
  u8 bEndpointAddress;
  u8 bmAttributes;
  u16 wMaxPacketSize;
  u8 bInterval;
  u8 pad[1];
};

constexpr u16 USBHDR(u8 dir, u8 type, u8 recipient, u8 request)
{
  return static_cast<u16>(((dir << 7 | type << 5 | recipient) << 8) | request);
}

constexpr int Align(const int num, const int alignment)
{
  return (num + (alignment - 1)) & ~(alignment - 1);
}

struct TransferCommand
{
  u32 cmd_address = 0;
  u32 data_addr = 0;
  s32 device_id = -1;

  void FillBuffer(const u8* src, size_t size) const;
  bool IsValid() const { return cmd_address != 0; }
  void Invalidate() { cmd_address = data_addr = 0; }
};

struct CtrlMessage : TransferCommand
{
  u8 bmRequestType = 0;
  u8 bmRequest = 0;
  u16 wValue = 0;
  u16 wIndex = 0;
  u16 wLength = 0;
};

struct BulkMessage : TransferCommand
{
  u16 length = 0;
  u8 endpoint = 0;
  void SetRetVal(const u32 retval) const;
};

struct IntrMessage : TransferCommand
{
  u16 length = 0;
  u8 endpoint = 0;
  void SetRetVal(const u32 retval) const;
};

struct IsoMessage : TransferCommand
{
  u32 packet_sizes_addr = 0;
  std::vector<u16> packet_sizes;
  u16 length = 0;
  u8 num_packets = 0;
  u8 endpoint = 0;
};

class Device
{
public:
  Device(u8 interface) : m_interface(interface) {}
  virtual ~Device() = default;
  s32 GetId() const { return m_id; }
  u16 GetVid() const { return m_vid; }
  u16 GetPid() const { return m_pid; }
  u8 GetInterfaceClass() const { return m_interface_class; }
  virtual std::string GetErrorName(int error_code) const;
  virtual bool AttachDevice() = 0;
  virtual int CancelTransfer(u8 endpoint) = 0;
  virtual int ChangeInterface(u8 interface) = 0;
  virtual int SetAltSetting(u8 alt_setting) = 0;
  virtual int SubmitTransfer(std::unique_ptr<CtrlMessage> message) = 0;
  virtual int SubmitTransfer(std::unique_ptr<BulkMessage> message) = 0;
  virtual int SubmitTransfer(std::unique_ptr<IntrMessage> message) = 0;
  virtual int SubmitTransfer(std::unique_ptr<IsoMessage> message) = 0;
  // Returns USB descriptors in the format used by IOS's USBV5 interface.
  virtual std::vector<u8> GetIOSDescriptors() = 0;

protected:
  s32 m_id = -1;
  u16 m_vid = 0;
  u16 m_pid = 0;
  u8 m_interface = 0;
  u8 m_interface_class = -1;
};
