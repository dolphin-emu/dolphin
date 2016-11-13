// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IPC_HLE/USB/Common.h"
#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"
#include "Core/HW/Memmap.h"

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

std::string USBDevice::GetErrorName(const int error_code) const
{
  return StringFromFormat("unknown error %d", error_code);
}
