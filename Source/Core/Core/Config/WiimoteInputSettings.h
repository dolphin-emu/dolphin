// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/Config/Config.h"

namespace Config
{
	// Configuration Information

	// WiimoteInput.Settings

	extern const ConfigInfo<double> WIIMOTE_INPUT_SWING_INTENSITY_FAST;
	extern const ConfigInfo<double> WIIMOTE_INPUT_SWING_INTENSITY_MEDIUM;
	extern const ConfigInfo<double> WIIMOTE_INPUT_SWING_INTENSITY_SLOW;

	extern const ConfigInfo<double> WIIMOTE_INPUT_SHAKE_INTENSITY_HARD;
	extern const ConfigInfo<double> WIIMOTE_INPUT_SHAKE_INTENSITY_MEDIUM;
	extern const ConfigInfo<double> WIIMOTE_INPUT_SHAKE_INTENSITY_SOFT;

  // NunchuckInput.Settings

  extern const ConfigInfo<double> NUNCHUK_INPUT_SWING_INTENSITY_FAST;
  extern const ConfigInfo<double> NUNCHUK_INPUT_SWING_INTENSITY_MEDIUM;
  extern const ConfigInfo<double> NUNCHUK_INPUT_SWING_INTENSITY_SLOW;

  extern const ConfigInfo<double> NUNCHUK_INPUT_SHAKE_INTENSITY_HARD;
  extern const ConfigInfo<double> NUNCHUK_INPUT_SHAKE_INTENSITY_MEDIUM;
  extern const ConfigInfo<double> NUNCHUK_INPUT_SHAKE_INTENSITY_SOFT;

  // Other settings

  extern const ConfigInfo<std::string> WIIMOTE_PROFILES;

}  // namespace Config
