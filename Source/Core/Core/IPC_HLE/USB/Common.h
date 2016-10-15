// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

class PointerWrap;
struct libusb_config_descriptor;
struct libusb_device;
struct libusb_device_descriptor;
struct libusb_endpoint_descriptor;
struct libusb_interface_descriptor;

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

void ConvertDeviceToWii(IOSDeviceDescriptor* dest, const libusb_device_descriptor* src);
void ConvertConfigToWii(IOSConfigDescriptor* dest, const libusb_config_descriptor* src);
void ConvertInterfaceToWii(IOSInterfaceDescriptor* dest, const libusb_interface_descriptor* src);
void ConvertEndpointToWii(IOSEndpointDescriptor* dest, const libusb_endpoint_descriptor* src);
int Align(int num, int alignment);

constexpr u16 USBHDR(u8 dir, u8 type, u8 recipient, u8 request)
{
  return static_cast<u16>(((dir << 7 | type << 5 | recipient) << 8) | request);
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
  std::vector<u16> packet_sizes;
  u16 length = 0;
  u8 num_packets = 0;
  u8 endpoint = 0;
};
