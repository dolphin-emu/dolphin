// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Core/HW/WiimoteCommon/WiimoteReport.h"

namespace WiimoteEmu
{
struct DesiredWiimoteState
{
  WiimoteCommon::ButtonData buttons{};  // non-button state in this is ignored
};
}  // namespace WiimoteEmu
