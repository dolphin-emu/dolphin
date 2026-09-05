// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <locale>

namespace Common
{
// std::locale{""} equivalent that doesn't throw
std::locale GetEnvironmentLocale();
}  // Namespace Common
