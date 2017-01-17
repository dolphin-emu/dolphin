// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

namespace IOS
{
namespace HLE
{
const u8* GetAttribPacket(u32 serviceHandle, u32 cont, u32& _size);
}  // namespace HLE
}  // namespace IOS
