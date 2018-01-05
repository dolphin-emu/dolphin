// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/USB/Common.h"

#include <algorithm>

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"
#include "Core/HW/Memmap.h"

namespace IOS
{
namespace HLE
{
namespace USB
{
std::unique_ptr<u8[]> TransferCommand::MakeBuffer(const size_t size) const
{
  _assert_msg_(IOS_USB, data_address != 0, "Invalid data_address");
  auto buffer = std::make_unique<u8[]>(size);
  Memory::CopyFromEmu(buffer.get(), data_address, size);
  return buffer;
}

void TransferCommand::FillBuffer(const u8* src, const size_t size) const
{
  _assert_msg_(IOS_USB, size == 0 || data_address != 0, "Invalid data_address");
  Memory::CopyToEmu(data_address, src, size);
}

void TransferCommand::OnTransferComplete(s32 return_value) const
{
  m_ios.EnqueueIPCReply(ios_request, return_value, 0, CoreTiming::FromThread::NON_CPU);
}

void IsoMessage::SetPacketReturnValue(const size_t packet_num, const u16 return_value) const
{
  Memory::Write_U16(return_value, static_cast<u32>(packet_sizes_addr + packet_num * sizeof(u16)));
}

Device::~Device() = default;

u64 Device::GetId() const
{
  return m_id;
}

u16 Device::GetVid() const
{
  return GetDeviceDescriptor().idVendor;
}

u16 Device::GetPid() const
{
  return GetDeviceDescriptor().idProduct;
}

bool Device::HasClass(const u8 device_class) const
{
  if (GetDeviceDescriptor().bDeviceClass == device_class)
    return true;
  const auto interfaces = GetInterfaces(0);
  return std::any_of(interfaces.begin(), interfaces.end(), [device_class](const auto& interface) {
    return interface.bInterfaceClass == device_class;
  });
}

static void CopyToBufferAligned(std::vector<u8>* buffer, const void* data, const size_t size)
{
  buffer->insert(buffer->end(), static_cast<const u8*>(data), static_cast<const u8*>(data) + size);
  const size_t number_of_padding_bytes = Common::AlignUp(size, 4) - size;
  buffer->insert(buffer->end(), number_of_padding_bytes, 0);
}

static void CopyDescriptorToBuffer(std::vector<u8>* buffer, DeviceDescriptor descriptor)
{
  descriptor.bcdUSB = Common::swap16(descriptor.bcdUSB);
  descriptor.idVendor = Common::swap16(descriptor.idVendor);
  descriptor.idProduct = Common::swap16(descriptor.idProduct);
  descriptor.bcdDevice = Common::swap16(descriptor.bcdDevice);
  CopyToBufferAligned(buffer, &descriptor, descriptor.bLength);
}

static void CopyDescriptorToBuffer(std::vector<u8>* buffer, ConfigDescriptor descriptor)
{
  descriptor.wTotalLength = Common::swap16(descriptor.wTotalLength);
  CopyToBufferAligned(buffer, &descriptor, descriptor.bLength);
}

static void CopyDescriptorToBuffer(std::vector<u8>* buffer, InterfaceDescriptor descriptor)
{
  CopyToBufferAligned(buffer, &descriptor, descriptor.bLength);
}

static void CopyDescriptorToBuffer(std::vector<u8>* buffer, EndpointDescriptor descriptor)
{
  descriptor.wMaxPacketSize = Common::swap16(descriptor.wMaxPacketSize);
  // IOS only copies 8 bytes from the endpoint descriptor, regardless of the actual length
  CopyToBufferAligned(buffer, &descriptor, sizeof(descriptor));
}

std::vector<u8> Device::GetDescriptorsUSBV4() const
{
  return GetDescriptors([](const auto& descriptor) { return true; });
}

std::vector<u8> Device::GetDescriptorsUSBV5(const u8 interface, const u8 alt_setting) const
{
  return GetDescriptors([interface, alt_setting](const auto& descriptor) {
    // The USBV5 interfaces present each interface as a different device,
    // and the descriptors are filtered by alternate setting.
    return descriptor.bInterfaceNumber == interface && descriptor.bAlternateSetting == alt_setting;
  });
}

std::vector<u8>
Device::GetDescriptors(std::function<bool(const InterfaceDescriptor&)> predicate) const
{
  std::vector<u8> buffer;

  const auto device_descriptor = GetDeviceDescriptor();
  CopyDescriptorToBuffer(&buffer, device_descriptor);

  const auto configurations = GetConfigurations();
  for (size_t c = 0; c < configurations.size(); ++c)
  {
    const auto& config_descriptor = configurations[c];
    CopyDescriptorToBuffer(&buffer, config_descriptor);

    const auto interfaces = GetInterfaces(static_cast<u8>(c));
    for (size_t i = interfaces.size(); i-- > 0;)
    {
      const auto& descriptor = interfaces[i];
      if (!predicate(descriptor))
        continue;

      CopyDescriptorToBuffer(&buffer, descriptor);
      for (const auto& endpoint_descriptor : GetEndpoints(
               static_cast<u8>(c), descriptor.bInterfaceNumber, descriptor.bAlternateSetting))
        CopyDescriptorToBuffer(&buffer, endpoint_descriptor);
    }
  }
  return buffer;
}

std::string Device::GetErrorName(const int error_code) const
{
  return StringFromFormat("unknown error %d", error_code);
}
}  // namespace USB
}  // namespace HLE
}  // namespace IOS
