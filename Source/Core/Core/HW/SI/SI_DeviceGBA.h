// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "Core/HW/SI/SI_Device.h"

// GameBoy Advance "Link Cable"

namespace SerialInterface
{
std::unique_ptr<ISIDevice> CreateSIDevice_GBA(Core::System& system, SIDevices device,
                                              int device_number);
}  // namespace SerialInterface
