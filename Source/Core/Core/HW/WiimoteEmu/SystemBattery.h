// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <optional>

namespace WiimoteEmu
{

#if defined(_WIN32) || defined(__APPLE__)

std::optional<float> GetSystemBatteryLevel(void);

#else

static inline constexpr std::optional<float> GetSystemBatteryLevel(void)
{
	return std::nullopt;
}

#endif

};
