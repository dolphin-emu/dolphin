// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include "Common/CommonTypes.h"

namespace Common
{
enum
{
  KBD_LAYOUT_QWERTY = 0,
  KBD_LAYOUT_AZERTY = 1
};

using HIDPressedKeys = std::array<u8, 6>;

bool IsVirtualKeyPressed(int virtual_key);
u8 PollHIDModifiers();
HIDPressedKeys PollHIDPressedKeys(int keyboard_layout);
}  // namespace Common
