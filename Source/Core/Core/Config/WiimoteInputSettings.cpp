// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/Config/WiimoteInputSettings.h"

namespace Config
{
// Configuration Information

// WiimoteInput.Settings

const ConfigInfo<double> WIIMOTE_INPUT_SWING_INTENSITY_FAST{{System::WiiPad, "Swing", "Fast"}, 4.5};
const ConfigInfo<double> WIIMOTE_INPUT_SWING_INTENSITY_MEDIUM{{System::WiiPad, "Swing", "Medium"},
                                                              2.5};
const ConfigInfo<double> WIIMOTE_INPUT_SWING_INTENSITY_SLOW{{System::WiiPad, "Swing", "Slow"}, 1.5};

const ConfigInfo<double> WIIMOTE_INPUT_SHAKE_INTENSITY_HARD{{System::WiiPad, "Shake", "Hard"}, 5.0};
const ConfigInfo<double> WIIMOTE_INPUT_SHAKE_INTENSITY_MEDIUM{{System::WiiPad, "Shake", "Medium"},
                                                              3.0};
const ConfigInfo<double> WIIMOTE_INPUT_SHAKE_INTENSITY_SOFT{{System::WiiPad, "Shake", "Soft"}, 2.0};

// Dynamic settings
const ConfigInfo<int> WIIMOTE_INPUT_SWING_DYNAMIC_FRAMES_HELD_FAST{
    {System::WiiPad, "Dynamic_Swing", "FramesHeldFast"}, 100};
const ConfigInfo<int> WIIMOTE_INPUT_SWING_DYNAMIC_FRAMES_HELD_SLOW{
    {System::WiiPad, "Dynamic_Swing", "FramesHeldSlow"}, 30};
const ConfigInfo<int> WIIMOTE_INPUT_SWING_DYNAMIC_FRAMES_LENGTH{
    {System::WiiPad, "Dynamic_Swing", "FrameCount"}, 30};

const ConfigInfo<int> WIIMOTE_INPUT_SHAKE_DYNAMIC_FRAMES_HELD_HARD{
    {System::WiiPad, "Dynamic_Shake", "FramesHeldHard"}, 45};
const ConfigInfo<int> WIIMOTE_INPUT_SHAKE_DYNAMIC_FRAMES_HELD_SOFT{
    {System::WiiPad, "Dynamic_Shake", "FramesHeldSoft"}, 15};
const ConfigInfo<int> WIIMOTE_INPUT_SHAKE_DYNAMIC_FRAMES_LENGTH{
    {System::WiiPad, "Dynamic_Shake", "FrameCount"}, 30};

// NunchuckInput.Settings

const ConfigInfo<double> NUNCHUK_INPUT_SWING_INTENSITY_FAST{
    {System::WiiPad, "Nunchuk_Swing", "Fast"}, 4.5};
const ConfigInfo<double> NUNCHUK_INPUT_SWING_INTENSITY_MEDIUM{
    {System::WiiPad, "Nunchuk_Swing", "Medium"}, 2.5};
const ConfigInfo<double> NUNCHUK_INPUT_SWING_INTENSITY_SLOW{
    {System::WiiPad, "Nunchuk_Swing", "Slow"}, 1.5};

const ConfigInfo<double> NUNCHUK_INPUT_SHAKE_INTENSITY_HARD{
    {System::WiiPad, "Nunchuk_Shake", "Hard"}, 5.0};
const ConfigInfo<double> NUNCHUK_INPUT_SHAKE_INTENSITY_MEDIUM{
    {System::WiiPad, "Nunchuk_Shake", "Medium"}, 3.0};
const ConfigInfo<double> NUNCHUK_INPUT_SHAKE_INTENSITY_SOFT{
    {System::WiiPad, "Nunchuk_Shake", "Soft"}, 2.0};
}
