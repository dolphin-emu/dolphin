// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "InputCommon/ControllerInterface/InputBackend.h"

namespace ciface::WiiUAdapter
{
std::unique_ptr<ciface::InputBackend> CreateInputBackend(ControllerInterface* controller_interface);

}  // namespace ciface::WiiUAdapter
