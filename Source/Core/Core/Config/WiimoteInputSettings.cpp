// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/Config/WiimoteInputSettings.h"

namespace Config
{
	// Configuration Information

	// WiimoteInput.Settings

	const ConfigInfo<double> WIIMOTE_INPUT_SWING_INTENSITY_FAST{ { System::WiiPad, "Swing", "Fast" }, 4.5 };
	const ConfigInfo<double> WIIMOTE_INPUT_SWING_INTENSITY_MEDIUM{ { System::WiiPad, "Swing", "Medium" }, 2.5 };
	const ConfigInfo<double> WIIMOTE_INPUT_SWING_INTENSITY_SLOW{ { System::WiiPad, "Swing", "Slow" }, 1.5 };

	const ConfigInfo<double> WIIMOTE_INPUT_SHAKE_INTENSITY_HARD{ { System::WiiPad, "Shake", "Hard" }, 5.0 };
	const ConfigInfo<double> WIIMOTE_INPUT_SHAKE_INTENSITY_MEDIUM{ { System::WiiPad, "Shake", "Medium" }, 3.0 };
	const ConfigInfo<double> WIIMOTE_INPUT_SHAKE_INTENSITY_SOFT{ { System::WiiPad, "Shake", "Soft" }, 2.0 };
}
