// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>

#include "Core/HW/WiimoteCommon/DataReport.h"
#include "InputCommon/ControllerEmu/ControlGroup/Buttons.h"
#include "InputCommon/ControllerEmu/ControlGroup/Force.h"
#include "InputCommon/ControllerEmu/ControlGroup/Tilt.h"

namespace WiimoteEmu
{
struct NormalizedAccelData
{
  // Unit is 1G
  double x, y, z;
};

// Used for a dynamic swing or shake
struct DynamicData
{
  std::array<int, 3> timing;                 // Hold length in frames for each axis
  std::array<double, 3> intensity;           // Swing or shake intensity
  std::array<int, 3> executing_frames_left;  // Number of frames to execute the intensity operation
};

// Used for a dynamic swing or shake.
// This is used to pass in data that defines the dynamic action
struct DynamicConfiguration
{
  double low_intensity;
  int frames_needed_for_low_intensity;

  double med_intensity;
  // Frames needed for med intensity can be calculated between high & low

  double high_intensity;
  int frames_needed_for_high_intensity;

  int frames_to_execute;  // How many frames should we execute the action for?
};

void EmulateShake(NormalizedAccelData* accel, ControllerEmu::Buttons* buttons_group,
                  double intensity, u8* shake_step);

void EmulateDynamicShake(NormalizedAccelData* accel, DynamicData& dynamic_data,
                         ControllerEmu::Buttons* buttons_group, const DynamicConfiguration& config,
                         u8* shake_step);

void EmulateTilt(NormalizedAccelData* accel, ControllerEmu::Tilt* tilt_group, bool sideways = false,
                 bool upright = false);

void EmulateSwing(NormalizedAccelData* accel, ControllerEmu::Force* swing_group, double intensity,
                  bool sideways = false, bool upright = false);

void EmulateDynamicSwing(NormalizedAccelData* accel, DynamicData& dynamic_data,
                         ControllerEmu::Force* swing_group, const DynamicConfiguration& config,
                         bool sideways = false, bool upright = false);

WiimoteCommon::DataReportBuilder::AccelData DenormalizeAccelData(const NormalizedAccelData& accel,
                                                                 u16 zero_g, u16 one_g);

}  // namespace WiimoteEmu
