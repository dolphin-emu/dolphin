// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

namespace IOS::HLE
{
const u8* GetAttribPacket(u32 serviceHandle, u32 cont, u32& _size);
}  // namespace IOS::HLE
