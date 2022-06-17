// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <memory>

struct cubeb;

namespace CubebUtils
{
void RunInCubebContext(const std::function<void()>& func);
std::shared_ptr<cubeb> GetContext();
}  // namespace CubebUtils
