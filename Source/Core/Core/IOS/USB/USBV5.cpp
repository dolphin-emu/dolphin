// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/USB/USBV5.h"

#include <cstddef>
#include <numeric>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/Device.h"

namespace IOS
{
namespace HLE
{
namespace USB
{
V5CtrlMessage::V5CtrlMessage(Kernel& ios, const IOCtlVRequest& ioctlv)
    : CtrlMessage(ios, ioctlv, Memory::Read_U32(ioctlv.in_vectors[0].address + 16))
{
  request_type = Memory::Read_U8(ioctlv.in_vectors[0].address + 8);
  request = Memory::Read_U8(ioctlv.in_vectors[0].address + 9);
  value = Memory::Read_U16(ioctlv.in_vectors[0].address + 10);
  index = Memory::Read_U16(ioctlv.in_vectors[0].address + 12);
  length = Memory::Read_U16(ioctlv.in_vectors[0].address + 14);
}

V5BulkMessage::V5BulkMessage(Kernel& ios, const IOCtlVRequest& ioctlv)
    : BulkMessage(ios, ioctlv, Memory::Read_U32(ioctlv.in_vectors[0].address + 8))
{
  length = Memory::Read_U16(ioctlv.in_vectors[0].address + 12);
  endpoint = Memory::Read_U8(ioctlv.in_vectors[0].address + 18);
}

V5IntrMessage::V5IntrMessage(Kernel& ios, const IOCtlVRequest& ioctlv)
    : IntrMessage(ios, ioctlv, Memory::Read_U32(ioctlv.in_vectors[0].address + 8))
{
  length = Memory::Read_U16(ioctlv.in_vectors[0].address + 12);
  endpoint = Memory::Read_U8(ioctlv.in_vectors[0].address + 14);
}

V5IsoMessage::V5IsoMessage(Kernel& ios, const IOCtlVRequest& ioctlv)
    : IsoMessage(ios, ioctlv, Memory::Read_U32(ioctlv.in_vectors[0].address + 8))
{
  num_packets = Memory::Read_U8(ioctlv.in_vectors[0].address + 16);
  endpoint = Memory::Read_U8(ioctlv.in_vectors[0].address + 17);
  packet_sizes_addr = Memory::Read_U32(ioctlv.in_vectors[0].address + 12);
  for (size_t i = 0; i < num_packets; ++i)
    packet_sizes.push_back(Memory::Read_U16(static_cast<u32>(packet_sizes_addr + i * sizeof(u16))));
  length = std::accumulate(packet_sizes.begin(), packet_sizes.end(), 0);
}
}  // namespace USB
}  // namespace HLE
}  // namespace IOS
