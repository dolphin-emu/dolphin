// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

namespace ciface
{
// Assumed to be u8. Actually used for output as well
enum class InputChannel : u8
{
  SerialInterface,  // GC Controllers
  Bluetooth,        // Wiimote and other BT devices
  Host,             // Hotkeys (worker thread) and UI (game input config panels, main thread)
  FreeLook,
  Max
};
}  // namespace ciface
