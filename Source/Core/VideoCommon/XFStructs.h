// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <utility>

#include "VideoCommon/XFMemory.h"

std::pair<std::string, std::string> GetXFRegInfo(u32 address, u32 value);
std::string GetXFMemName(u32 address);
std::string GetXFMemDescription(u32 address, u32 value);
std::pair<std::string, std::string> GetXFTransferInfo(u16 base_address, u8 transfer_size,
                                                      const u8* data);
std::pair<std::string, std::string> GetXFIndexedLoadInfo(CPArray array, u32 index, u16 address,
                                                         u8 size);
