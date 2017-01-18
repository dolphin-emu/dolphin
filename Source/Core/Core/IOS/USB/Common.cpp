// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/USB/Common.h"
#include "Common/Assert.h"
#include "Common/CommonTypes.h"
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
}  // namespace USB
}  // namespace HLE
}  // namespace IOS
