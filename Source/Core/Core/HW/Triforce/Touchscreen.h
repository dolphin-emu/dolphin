// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Core/HW/Triforce/SerialDevice.h"

namespace Triforce
{

// The touchscreen input used by The Key of Avalon games.
class Touchscreen final : public SerialDevice
{
public:
  void Update() override;
};

}  // namespace Triforce
