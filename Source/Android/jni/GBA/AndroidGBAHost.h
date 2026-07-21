// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "Core/Host.h"

namespace HW::GBA
{
class Core;
}

std::unique_ptr<GBAHostInterface> CreateAndroidGBAHost(std::weak_ptr<HW::GBA::Core> core);
