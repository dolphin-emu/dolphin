// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>

#include "Common/MathUtil.h"
#include "Common/Matrix.h"
#include "Core/HW/WiimoteCommon/DataReport.h"
#include "InputCommon/ControllerEmu/ControlGroup/Buttons.h"
#include "InputCommon/ControllerEmu/ControlGroup/Cursor.h"
#include "InputCommon/ControllerEmu/ControlGroup/Force.h"
#include "InputCommon/ControllerEmu/ControlGroup/IMUAccelerometer.h"
#include "InputCommon/ControllerEmu/ControlGroup/IMUCursor.h"
#include "InputCommon/ControllerEmu/ControlGroup/IMUGyroscope.h"
#include "InputCommon/ControllerEmu/ControlGroup/Tilt.h"

namespace WiimoteEmu
{
using MathUtil::GRAVITY_ACCELERATION;

struct PositionalState
{
  // meters
  Common::Vec3 position;
  // meters/second
  Common::Vec3 velocity;
  // meters/second^2
  Common::Vec3 acceleration;
};

struct RotationalState
{
  // radians
  Common::Vec3 angle;
  // radians/second
  Common::Vec3 angular_velocity;
};

struct IMUCursorState
{
  IMUCursorState();

  // Rotation of world around device.
  Common::Matrix33 rotation;

  float recentered_pitch = {};
};

// Contains both positional and rotational state.
struct MotionState : PositionalState, RotationalState
{
};

// Estimate orientation from accelerometer data.
Common::Matrix33 GetMatrixFromAcceleration(const Common::Vec3& accel);

// Build a rotational matrix from euler angles.
Common::Matrix33 GetRotationalMatrix(const Common::Vec3& angle);

void ApproachPositionWithJerk(PositionalState* state, const Common::Vec3& target,
                              const Common::Vec3& max_jerk, float time_elapsed);

void ApproachAngleWithAccel(RotationalState* state, const Common::Vec3& target, float max_accel,
                            float time_elapsed);

void EmulateShake(PositionalState* state, ControllerEmu::Shake* shake_group, float time_elapsed);
void EmulateTilt(RotationalState* state, ControllerEmu::Tilt* tilt_group, float time_elapsed);
void EmulateSwing(MotionState* state, ControllerEmu::Force* swing_group, float time_elapsed);
void EmulateCursor(MotionState* state, ControllerEmu::Cursor* ir_group, float time_elapsed);
void EmulateIMUCursor(IMUCursorState* state, ControllerEmu::IMUCursor* imu_ir_group,
                      ControllerEmu::IMUAccelerometer* imu_accelerometer_group,
                      ControllerEmu::IMUGyroscope* imu_gyroscope_group, float time_elapsed);

// Convert m/s/s acceleration data to the format used by Wiimote/Nunchuk (10-bit unsigned integers).
WiimoteCommon::DataReportBuilder::AccelData ConvertAccelData(const Common::Vec3& accel, u16 zero_g,
                                                             u16 one_g);

}  // namespace WiimoteEmu
