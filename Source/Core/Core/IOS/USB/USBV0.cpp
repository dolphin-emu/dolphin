// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/USBV0.h"

#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Swap.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/Device.h"
#include "Core/System.h"

namespace IOS::HLE::USB
{
V0CtrlMessage::V0CtrlMessage(EmulationKernel& ios, const IOCtlVRequest& ioctlv)
    : CtrlMessage(ios, ioctlv, ioctlv.io_vectors[0].address)
{
  auto& system = ios.GetSystem();
  auto& memory = system.GetMemory();
  request_type = memory.Read_U8(ioctlv.in_vectors[0].address);
  request = memory.Read_U8(ioctlv.in_vectors[1].address);
  value = Common::swap16(memory.Read_U16(ioctlv.in_vectors[2].address));
  index = Common::swap16(memory.Read_U16(ioctlv.in_vectors[3].address));
  length = Common::swap16(memory.Read_U16(ioctlv.in_vectors[4].address));
}

V0BulkMessage::V0BulkMessage(EmulationKernel& ios, const IOCtlVRequest& ioctlv, bool long_length)
    : BulkMessage(ios, ioctlv, ioctlv.io_vectors[0].address)
{
  auto& system = ios.GetSystem();
  auto& memory = system.GetMemory();
  endpoint = memory.Read_U8(ioctlv.in_vectors[0].address);
  if (long_length)
    length = memory.Read_U32(ioctlv.in_vectors[1].address);
  else
    length = memory.Read_U16(ioctlv.in_vectors[1].address);
}

V0IntrMessage::V0IntrMessage(EmulationKernel& ios, const IOCtlVRequest& ioctlv)
    : IntrMessage(ios, ioctlv, ioctlv.io_vectors[0].address)
{
  auto& system = ios.GetSystem();
  auto& memory = system.GetMemory();
  endpoint = memory.Read_U8(ioctlv.in_vectors[0].address);
  length = memory.Read_U16(ioctlv.in_vectors[1].address);
}

V0IsoMessage::V0IsoMessage(EmulationKernel& ios, const IOCtlVRequest& ioctlv)
    : IsoMessage(ios, ioctlv, ioctlv.io_vectors[1].address)
{
  auto& system = ios.GetSystem();
  auto& memory = system.GetMemory();
  endpoint = memory.Read_U8(ioctlv.in_vectors[0].address);
  length = memory.Read_U16(ioctlv.in_vectors[1].address);
  num_packets = memory.Read_U8(ioctlv.in_vectors[2].address);
  packet_sizes_addr = ioctlv.io_vectors[0].address;
  for (size_t i = 0; i < num_packets; ++i)
    packet_sizes.push_back(memory.Read_U16(static_cast<u32>(packet_sizes_addr + i * sizeof(u16))));
}
}  // namespace IOS::HLE::USB
