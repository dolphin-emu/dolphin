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

// Below settings are for dynamic input only (based on how long the user holds a button)

extern const ConfigInfo<int>
    WIIMOTE_INPUT_SWING_DYNAMIC_FRAMES_HELD_FAST;  // How long button held constitutes a fast swing
extern const ConfigInfo<int>
    WIIMOTE_INPUT_SWING_DYNAMIC_FRAMES_HELD_SLOW;  // How long button held constitutes a slow swing
extern const ConfigInfo<int>
    WIIMOTE_INPUT_SWING_DYNAMIC_FRAMES_LENGTH;  // How long to execute the swing

extern const ConfigInfo<int>
    WIIMOTE_INPUT_SHAKE_DYNAMIC_FRAMES_HELD_HARD;  // How long button held constitutes a hard shake
extern const ConfigInfo<int>
    WIIMOTE_INPUT_SHAKE_DYNAMIC_FRAMES_HELD_SOFT;  // How long button held constitutes a soft shake
extern const ConfigInfo<int>
    WIIMOTE_INPUT_SHAKE_DYNAMIC_FRAMES_LENGTH;  // How long to execute a shake

// NunchuckInput.Settings

extern const ConfigInfo<double> NUNCHUK_INPUT_SWING_INTENSITY_FAST;
extern const ConfigInfo<double> NUNCHUK_INPUT_SWING_INTENSITY_MEDIUM;
extern const ConfigInfo<double> NUNCHUK_INPUT_SWING_INTENSITY_SLOW;

extern const ConfigInfo<double> NUNCHUK_INPUT_SHAKE_INTENSITY_HARD;
extern const ConfigInfo<double> NUNCHUK_INPUT_SHAKE_INTENSITY_MEDIUM;
extern const ConfigInfo<double> NUNCHUK_INPUT_SHAKE_INTENSITY_SOFT;

}  // namespace Config
