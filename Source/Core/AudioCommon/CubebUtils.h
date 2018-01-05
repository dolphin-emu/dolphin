// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

struct cubeb;

namespace CubebUtils
{
std::shared_ptr<cubeb> GetContext();
}  // namespace CubebUtils
