// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "Core/HW/EXI/EXI_Device.h"

namespace ExpansionInterface
{
std::unique_ptr<IEXIDevice> CreateEXIGecko(Core::System& system);
}  // namespace ExpansionInterface
