// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/Common.h"

#include <algorithm>

#include <fmt/format.h>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Swap.h"
#include "Core/HW/Memmap.h"
#include "Core/System.h"

namespace IOS::HLE::USB
{
std::unique_ptr<u8[]> TransferCommand::MakeBuffer(const size_t size) const
{
  ASSERT_MSG(IOS_USB, data_address != 0, "Invalid data_address");
  auto buffer = std::make_unique<u8[]>(size);
  auto& system = m_ios.GetSystem();
  auto& memory = system.GetMemory();
  memory.CopyFromEmu(buffer.get(), data_address, size);
  return buffer;
}

void TransferCommand::FillBuffer(const u8* src, const size_t size) const
{
  ASSERT_MSG(IOS_USB, size == 0 || data_address != 0, "Invalid data_address");
  auto& system = m_ios.GetSystem();
  auto& memory = system.GetMemory();
  memory.CopyToEmu(data_address, src, size);
}

void TransferCommand::OnTransferComplete(s32 return_value) const
{
  m_ios.EnqueueIPCReply(ios_request, return_value, 0, CoreTiming::FromThread::NON_CPU);
}

void TransferCommand::ScheduleTransferCompletion(s32 return_value, u32 expected_time_us) const
{
  auto ticks = m_ios.GetSystem().GetSystemTimers().GetTicksPerSecond();
  s64 cycles_in_future = static_cast<s64>((static_cast<u64>(ticks) * expected_time_us) / 1'000'000);
  m_ios.EnqueueIPCReply(ios_request, return_value, cycles_in_future, CoreTiming::FromThread::ANY);
}

void IsoMessage::SetPacketReturnValue(const size_t packet_num, const u16 return_value) const
{
  auto& system = m_ios.GetSystem();
  auto& memory = system.GetMemory();
  memory.Write_U16(return_value, static_cast<u32>(packet_sizes_addr + packet_num * sizeof(u16)));
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

void DeviceDescriptor::Swap()
{
  bcdUSB = Common::swap16(bcdUSB);
  idVendor = Common::swap16(idVendor);
  idProduct = Common::swap16(idProduct);
  bcdDevice = Common::swap16(bcdDevice);
}

void ConfigDescriptor::Swap()
{
  wTotalLength = Common::swap16(wTotalLength);
}

void InterfaceDescriptor::Swap()
{
}

void EndpointDescriptor::Swap()
{
  wMaxPacketSize = Common::swap16(wMaxPacketSize);
}

std::string Device::GetErrorName(const int error_code) const
{
  return fmt::format("unknown error {}", error_code);
}
}  // namespace IOS::HLE::USB
