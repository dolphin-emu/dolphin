// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>

#include "Common/Matrix.h"
#include "Core/HW/WiimoteCommon/DataReport.h"
#include "InputCommon/ControllerEmu/ControlGroup/Buttons.h"
#include "InputCommon/ControllerEmu/ControlGroup/Cursor.h"
#include "InputCommon/ControllerEmu/ControlGroup/Force.h"
#include "InputCommon/ControllerEmu/ControlGroup/Tilt.h"

namespace WiimoteEmu
{
constexpr double GRAVITY_ACCELERATION = 9.80665;

// Used for dynamic shake
// TODO: kill this.
struct DynamicData
{
  std::array<int, 3> timing;                 // Hold length in frames for each axis
  std::array<double, 3> intensity;           // Swing or shake intensity
  std::array<int, 3> executing_frames_left;  // Number of frames to execute the intensity operation
};

struct PositionalState
{
  Common::Vec3 position;
  Common::Vec3 velocity;
  Common::Vec3 acceleration;
};

struct RotationalState
{
  Common::Vec3 angle;
  Common::Vec3 angular_velocity;
};

// Contains both positional and rotational state.
struct MotionState : PositionalState, RotationalState
{
};

// Build a rotational matrix from euler angles.
Common::Matrix33 GetRotationalMatrix(const Common::Vec3& angle);

void ApproachPositionWithJerk(PositionalState* state, const Common::Vec3& target, float max_jerk,
                              float time_elapsed);

void ApproachAngleWithAccel(RotationalState* state, const Common::Vec3& target, float max_accel,
                            float time_elapsed);

// Used for dynamic shake.
// This is used to pass in data that defines the dynamic action
// TODO: kill this
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

Common::Vec3 EmulateShake(ControllerEmu::Buttons* buttons_group, double intensity, u8* shake_step);

Common::Vec3 EmulateDynamicShake(DynamicData& dynamic_data, ControllerEmu::Buttons* buttons_group,
                                 const DynamicConfiguration& config, u8* shake_step);

void EmulateTilt(RotationalState* state, ControllerEmu::Tilt* tilt_group, float time_elapsed);
void EmulateSwing(MotionState* state, ControllerEmu::Force* swing_group, float time_elapsed);

// Convert m/s/s acceleration data to the format used by Wiimote/Nunchuk (10-bit unsigned integers).
WiimoteCommon::DataReportBuilder::AccelData ConvertAccelData(const Common::Vec3& accel, u16 zero_g,
                                                             u16 one_g);

Common::Matrix44 EmulateCursorMovement(ControllerEmu::Cursor* ir_group);

}  // namespace WiimoteEmu
