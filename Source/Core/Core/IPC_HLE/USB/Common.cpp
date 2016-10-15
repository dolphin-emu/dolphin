// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>

#include "Common/Assert.h"
#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Core/HW/Memmap.h"
#include "Core/IPC_HLE/USB/Common.h"

void ConvertDeviceToWii(IOSDeviceDescriptor* dest, const libusb_device_descriptor* src)
{
  memcpy(dest, src, sizeof(IOSDeviceDescriptor));
  dest->bcdUSB = Common::swap16(dest->bcdUSB);
  dest->idVendor = Common::swap16(dest->idVendor);
  dest->idProduct = Common::swap16(dest->idProduct);
  dest->bcdDevice = Common::swap16(dest->bcdDevice);
}

void ConvertConfigToWii(IOSConfigDescriptor* dest, const libusb_config_descriptor* src)
{
  memcpy(dest, src, sizeof(IOSConfigDescriptor));
  dest->wTotalLength = Common::swap16(dest->wTotalLength);
}

void ConvertInterfaceToWii(IOSInterfaceDescriptor* dest, const libusb_interface_descriptor* src)
{
  memcpy(dest, src, sizeof(IOSInterfaceDescriptor));
}

void ConvertEndpointToWii(IOSEndpointDescriptor* dest, const libusb_endpoint_descriptor* src)
{
  memcpy(dest, src, sizeof(IOSEndpointDescriptor));
  dest->wMaxPacketSize = Common::swap16(dest->wMaxPacketSize);
}

int Align(const int num, const int alignment)
{
  return (num + (alignment - 1)) & ~(alignment - 1);
}

void TransferCommand::FillBuffer(const u8* src, const size_t size) const
{
  _assert_msg_(WII_IPC_USB, data_addr != 0, "Invalid data_addr");
  Memory::CopyToEmu(data_addr, src, size);
}

void BulkMessage::SetRetVal(const u32 retval) const
{
  Memory::Write_U32(retval, cmd_address + 4);
}

void IntrMessage::SetRetVal(const u32 retval) const
{
  Memory::Write_U32(retval, cmd_address + 4);
}
