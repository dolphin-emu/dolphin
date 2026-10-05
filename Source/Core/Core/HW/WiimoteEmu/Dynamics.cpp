// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/WiimoteEmu/Dynamics.h"

#include <algorithm>
#include <cmath>
#include <optional>

#include "Common/MathUtil.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "InputCommon/ControllerEmu/ControlGroup/Cursor.h"
#include "InputCommon/ControllerEmu/ControlGroup/Force.h"
#include "InputCommon/ControllerEmu/ControlGroup/IMUAccelerometer.h"
#include "InputCommon/ControllerEmu/ControlGroup/IMUCursor.h"
#include "InputCommon/ControllerEmu/ControlGroup/IMUGyroscope.h"
#include "InputCommon/ControllerEmu/ControlGroup/Tilt.h"

namespace
{
// Given a velocity, acceleration, and maximum jerk value,
// calculate change in position after a stop in the shortest possible time.
// Used to smoothly adjust acceleration and come to complete stops at precise positions.
// Based on equations for motion with constant jerk.
// s = s0 + v0 t + a0 t^2 / 2 + j t^3 / 6
double CalculateStopDistance(double velocity, double acceleration, double max_jerk)
{
  // Math below expects velocity to be non-negative.
  const auto velocity_flip = (velocity < 0 ? -1 : 1);

  const auto v_0 = velocity * velocity_flip;
  const auto a_0 = acceleration * velocity_flip;
  const auto j = max_jerk;

  // Time to reach zero acceleration.
  const auto t_0 = a_0 / j;

  // Distance to reach zero acceleration.
  const auto d_0 = std::pow(a_0, 3) / (3 * j * j) + (a_0 * v_0) / j;

  // Velocity at zero acceleration.
  const auto v_1 = v_0 + a_0 * std::abs(t_0) - std::copysign(j * t_0 * t_0 / 2, t_0);

  // Distance to complete stop.
  const auto d_1 = std::copysign(std::pow(std::abs(v_1), 3.0 / 2), v_1) / std::sqrt(j);

  return (d_0 + d_1) * velocity_flip;
}

double CalculateStopDistance(double velocity, double max_accel)
{
  return velocity * velocity / (2 * std::copysign(max_accel, velocity));
}

}  // namespace

namespace WiimoteEmu
{
Common::Quaternion ComplementaryFilter(const Common::Quaternion& gyroscope,
                                       const Common::Vec3& accelerometer, float accel_weight,
                                       const Common::Vec3& accelerometer_normal)
{
  const auto gyro_vec = gyroscope * accelerometer_normal;
  const auto normalized_accel = accelerometer.Normalized();

  const auto cos_angle = normalized_accel.Dot(gyro_vec);

  // If gyro to accel angle difference is between 0 and 180 degrees we make an adjustment.
  const auto abs_cos_angle = std::abs(cos_angle);
  if (abs_cos_angle > 0 && abs_cos_angle < 1)
  {
    const auto axis = gyro_vec.Cross(normalized_accel).Normalized();
    return Common::Quaternion::Rotate(std::acos(cos_angle) * accel_weight, axis) * gyroscope;
  }
  else
  {
    return gyroscope;
  }
}

void EmulateShake(PositionalState* state, ControllerEmu::Shake* const shake_group,
                  float time_elapsed)
{
  auto target_position = shake_group->GetState() * float(shake_group->GetIntensity() / 2);
  for (std::size_t i = 0; i != target_position.data.size(); ++i)
  {
    if (state->velocity.data[i] * std::copysign(1.f, target_position.data[i]) < 0 ||
        state->position.data[i] / target_position.data[i] > 0.5)
    {
      target_position.data[i] *= -1;
    }
  }

  // Time from "top" to "bottom" of one shake.
  const auto travel_time = 1 / shake_group->GetFrequency() / 2;

  Common::Vec3 jerk;
  for (std::size_t i = 0; i != target_position.data.size(); ++i)
  {
    const auto half_distance =
        std::max(std::abs(target_position.data[i]), std::abs(state->position.data[i]));

    jerk.data[i] = half_distance / std::pow(travel_time / 2, 3);
  }

  ApproachPositionWithJerk(state, target_position, jerk, time_elapsed);
}

void EmulateTilt(RotationalState* state, ControllerEmu::Tilt* const tilt_group, float time_elapsed)
{
  const auto target = tilt_group->GetState();

  // 180 degrees is currently the max tilt value.
  const ControlState roll = target.x * MathUtil::PI;
  const ControlState pitch = target.y * MathUtil::PI;

  const auto target_angle = Common::Vec3(pitch, -roll, 0);

  // For each axis, wrap around current angle if target is farther than 180 degrees.
  for (std::size_t i = 0; i != target_angle.data.size(); ++i)
  {
    auto& angle = state->angle.data[i];
    if (std::abs(angle - target_angle.data[i]) > float(MathUtil::PI))
      angle -= std::copysign(MathUtil::TAU, angle);
  }

  const auto max_accel = std::pow(tilt_group->GetMaxRotationalVelocity(), 2) / MathUtil::TAU;

  ApproachAngleWithAccel(state, target_angle, max_accel, time_elapsed);
}

Common::Quaternion GetSwingRotation(const MotionState& state)
{
  return (Common::Quaternion::RotateX(state.angle.x) * Common::Quaternion::RotateZ(state.angle.z))
      .Normalized();
}

void EmulateSwing(MotionState* state, ControllerEmu::Force* swing_group, float time_elapsed,
                  std::optional<Common::Vec2> point_angles)
{
  if (time_elapsed <= 0)
    return;

  const bool relative = swing_group->IsRelativeInput();
  const bool recenter = relative && swing_group->IsRecenterPressed();
  const bool pointing = relative && point_angles && !recenter;
  const float arm_length = swing_group->GetMaxDistance();
  const float configured_angle = swing_group->GetTwistAngle();
  const float max_angle =
      relative ? std::min(float(MathUtil::TAU * 85 / 360), configured_angle) : configured_angle;
  constexpr float RESPONSE_TIME = 0.02f;
  float response_time = RESPONSE_TIME;
  float movement_speed = swing_group->GetSpeed();
  Common::Vec3 coordinates{-state->angle.z, state->angle.x, state->forward_offset};
  const float old_forward_offset = state->forward_offset;
  Common::Vec3 requested_velocity{};
  std::optional<Common::Vec3> target;

  // Only input interpretation differs. All inputs drive the same rate filter,
  // bounded pose, arm kinematics and sensor derivatives below.
  if (!relative)
  {
    const auto input = swing_group->GetState();
    target =
        Common::Vec3{input.x / arm_length * max_angle, -input.y / arm_length * max_angle, input.z};
    const float extent = std::clamp(
        std::max(Common::Vec2{input.x, input.y}.Length(), std::abs(input.z)) / arm_length, 0.f,
        1.f);
    movement_speed = MathUtil::Lerp(float(swing_group->GetReturnSpeed()), movement_speed, extent);
  }
  else if (recenter)
  {
    target = Common::Vec3{};
    response_time = float(0.48 * arm_length / swing_group->GetReturnSpeed());
  }
  else if (pointing)
  {
    target = Common::Vec3{point_angles->x, point_angles->y, 0};
    // Point already has its own filter. Only limit the return from a gesture.
    response_time = time_elapsed;
  }
  else
  {
    const auto input = swing_group->GetRelativeState();
    const float sensitivity = swing_group->GetSensitivity();
    requested_velocity =
        Common::Vec3{input.x, -input.y, input.z * arm_length} * (sensitivity / time_elapsed);
  }

  const Common::Vec3 limits{max_angle, max_angle, arm_length};
  if (target)
  {
    for (std::size_t i = 0; i != target->data.size(); ++i)
      target->data[i] = std::clamp(target->data[i], -limits.data[i], limits.data[i]);
    // Small position changes produce proportionally small rates and accelerations,
    // rather than the distance-independent jerk previously used by Swing.
    requested_velocity = (*target - coordinates) / response_time;
  }
  const float max_angular_velocity = pointing ? float(swing_group->GetReturnSpeed() * 3) :
                                                std::min(20.f, movement_speed / arm_length);
  const float speed = Common::Vec2{requested_velocity.x, requested_velocity.y}.Length();
  if (speed > max_angular_velocity)
  {
    requested_velocity.x *= max_angular_velocity / speed;
    requested_velocity.y *= max_angular_velocity / speed;
  }
  requested_velocity.z = std::clamp(requested_velocity.z, -movement_speed, movement_speed);

  const auto old_rotation = GetSwingRotation(*state);
  const auto old_angular_velocity = state->angular_velocity;
  const Common::Vec3 arm{0, -arm_length, 0};
  const float old_forward_velocity =
      (old_rotation * old_angular_velocity.Cross(arm)).y - state->velocity.y;
  const float filter = pointing ? 1.f : time_elapsed / (RESPONSE_TIME + time_elapsed);
  for (std::size_t i = 0; i != coordinates.data.size(); ++i)
  {
    auto& velocity = state->input_velocity.data[i];
    if (!recenter && velocity * requested_velocity.data[i] < 0)
      velocity = 0;
    velocity += (requested_velocity.data[i] - velocity) * filter;
    if (std::abs(velocity) < 1e-5f)
      velocity = 0;
    float next = coordinates.data[i] + velocity * time_elapsed;
    if (target && (next - target->data[i]) * (coordinates.data[i] - target->data[i]) <= 0)
    {
      next = target->data[i];
      velocity = 0;
    }
    coordinates.data[i] = std::clamp(next, -limits.data[i], limits.data[i]);
    // Discard overflow so a reversal at a limit responds immediately.
    if (coordinates.data[i] != next)
      velocity = 0;
  }
  state->angle = {coordinates.y, 0, -coordinates.x};
  state->forward_offset = coordinates.z;
  const auto rotation = GetSwingRotation(*state);
  const auto change = (old_rotation.Conjugate() * rotation).Normalized();
  const Common::Vec3 axis{change.data.x, change.data.y, change.data.z};
  const float length = axis.Length();
  state->angular_velocity =
      length > 1e-8f ? axis * (2 * std::atan2(length, std::abs(change.data.w)) /
                               (length * time_elapsed) * std::copysign(1.f, change.data.w)) :
                       Common::Vec3{};

  // Every Swing uses the same outstretched arm and pose-derived sensor data.
  const auto angular_acceleration = (state->angular_velocity - old_angular_velocity) / time_elapsed;
  const auto velocity = state->angular_velocity.Cross(arm);
  const float forward_velocity = (state->forward_offset - old_forward_offset) / time_elapsed;
  state->position = rotation * arm - arm + Common::Vec3{0, -state->forward_offset, 0};
  state->velocity = rotation * velocity + Common::Vec3{0, -forward_velocity, 0};
  state->acceleration =
      rotation * (angular_acceleration.Cross(arm) + state->angular_velocity.Cross(velocity));
  state->acceleration.y -= (forward_velocity - old_forward_velocity) / time_elapsed;
  if (pointing)
  {
    // Ordinary pointing is wrist rotation, not a swing of the whole arm.
    state->velocity = {};
    state->acceleration = {};
  }
}

WiimoteCommon::AccelData ConvertAccelData(const Common::Vec3& accel, u16 zero_g, u16 one_g)
{
  const auto scaled_accel = accel * (one_g - zero_g) / float(GRAVITY_ACCELERATION);

  // 10-bit integers.
  constexpr long MAX_VALUE = (1 << 10) - 1;

  return WiimoteCommon::AccelData(
      {u16(std::clamp(std::lround(scaled_accel.x + zero_g), 0l, MAX_VALUE)),
       u16(std::clamp(std::lround(scaled_accel.y + zero_g), 0l, MAX_VALUE)),
       u16(std::clamp(std::lround(scaled_accel.z + zero_g), 0l, MAX_VALUE))});
}

void EmulatePoint(MotionState* state, ControllerEmu::Cursor* ir_group,
                  const ControllerEmu::InputOverrideFunction& override_func, float time_elapsed)
{
  const auto cursor = ir_group->GetState(true, override_func);

  if (!cursor.IsVisible())
  {
    // Move the wiimote a kilometer forward so the sensor bar is always behind it.
    *state = {};
    state->position = {0, -1000, 0};
    return;
  }

  // Nintendo recommends a distance of 1-3 meters.
  constexpr float NEUTRAL_DISTANCE = 2.f;

  // When the sensor bar position is on bottom, apply the "offset" setting negatively.
  // This is kinda odd but it does seem to maintain consistent cursor behavior.
  const bool sensor_bar_on_top = Config::Get(Config::SYSCONF_SENSOR_BAR_POSITION) != 0;

  const float height = ir_group->GetVerticalOffset() * (sensor_bar_on_top ? 1 : -1);

  const float yaw_scale = ir_group->GetTotalYaw() / 2;
  const float pitch_scale = ir_group->GetTotalPitch() / 2;

  // Just jump to the target position.
  state->position = {0, NEUTRAL_DISTANCE, -height};
  state->velocity = {};
  state->acceleration = {};

  const auto target_angle = Common::Vec3(pitch_scale * -cursor.y, 0, yaw_scale * -cursor.x);

  // If cursor was hidden, jump to the target angle immediately.
  if (state->position.y < 0)
  {
    state->angle = target_angle;
    state->angular_velocity = {};

    return;
  }

  // Higher values will be more responsive but increase rate of M+ "desync".
  // I'd rather not expose this value in the UI if not needed.
  // At this value, sync is very good and responsiveness still appears instant.
  constexpr auto MAX_ACCEL = float(MathUtil::TAU * 8);

  ApproachAngleWithAccel(state, target_angle, MAX_ACCEL, time_elapsed);
}

void ApproachAngleWithAccel(RotationalState* state, const Common::Vec3& angle_target,
                            float max_accel, float time_elapsed)
{
  const auto stop_distance =
      Common::Vec3(CalculateStopDistance(state->angular_velocity.x, max_accel),
                   CalculateStopDistance(state->angular_velocity.y, max_accel),
                   CalculateStopDistance(state->angular_velocity.z, max_accel));

  const auto offset = angle_target - state->angle;
  const auto stop_offset = offset - stop_distance;
  const auto accel = MathUtil::Sign(stop_offset) * max_accel;

  state->angular_velocity += accel * time_elapsed;

  const auto change_in_angle =
      state->angular_velocity * time_elapsed + accel * time_elapsed * time_elapsed / 2;

  for (std::size_t i = 0; i != offset.data.size(); ++i)
  {
    // If new angle will overshoot stop right on target.
    if (std::abs(offset.data[i]) < 0.0001 || (change_in_angle.data[i] / offset.data[i] > 1.0))
    {
      state->angular_velocity.data[i] =
          (angle_target.data[i] - state->angle.data[i]) / time_elapsed;
      state->angle.data[i] = angle_target.data[i];
    }
    else
    {
      state->angle.data[i] += change_in_angle.data[i];
    }
  }
}

void EmulateIMUCursor(IMUCursorState* state, ControllerEmu::IMUCursor* imu_ir_group,
                      ControllerEmu::IMUAccelerometer* imu_accelerometer_group,
                      ControllerEmu::IMUGyroscope* imu_gyroscope_group, float time_elapsed)
{
  const auto ang_vel = imu_gyroscope_group->GetState();

  // Reset if pointing is disabled or we have no gyro data.
  if (!imu_ir_group->enabled.GetValue() || !ang_vel.has_value())
  {
    *state = {};
    return;
  }

  // Apply rotation from gyro data.
  const auto gyro_rotation = GetRotationFromGyroscope(*ang_vel * -1 * time_elapsed);
  state->rotation = gyro_rotation * state->rotation;

  // If we have some non-zero accel data use it to adjust gyro drift.
  const auto accel_weight = imu_ir_group->GetAccelWeight();
  auto const accel = imu_accelerometer_group->GetState().value_or(Common::Vec3{});
  if (accel.LengthSquared())
    state->rotation = ComplementaryFilter(state->rotation, accel, accel_weight);

  // Clamp yaw within configured bounds.
  const auto yaw = GetYaw(state->rotation);
  const auto max_yaw = float(imu_ir_group->GetTotalYaw() / 2);
  auto target_yaw = std::clamp(yaw, -max_yaw, max_yaw);

  // Handle the "Recenter" button being pressed.
  if (imu_ir_group->controls[0]->GetState<bool>())
  {
    state->recentered_pitch = GetPitch(state->rotation);
    target_yaw = 0;
  }

  // Adjust yaw as needed.
  if (yaw != target_yaw)
    state->rotation *= Common::Quaternion::RotateZ(target_yaw - yaw);

  // Normalize for floating point inaccuracies.
  state->rotation = state->rotation.Normalized();
}

void ApproachPositionWithJerk(PositionalState* state, const Common::Vec3& position_target,
                              const Common::Vec3& max_jerk, float time_elapsed)
{
  const auto stop_distance =
      Common::Vec3(CalculateStopDistance(state->velocity.x, state->acceleration.x, max_jerk.x),
                   CalculateStopDistance(state->velocity.y, state->acceleration.y, max_jerk.y),
                   CalculateStopDistance(state->velocity.z, state->acceleration.z, max_jerk.z));

  const auto offset = position_target - state->position;
  const auto stop_offset = offset - stop_distance;
  const auto jerk = MathUtil::Sign(stop_offset) * max_jerk;

  state->acceleration += jerk * time_elapsed;

  state->velocity += state->acceleration * time_elapsed + jerk * time_elapsed * time_elapsed / 2;

  const auto change_in_position = state->velocity * time_elapsed +
                                  state->acceleration * time_elapsed * time_elapsed / 2 +
                                  jerk * time_elapsed * time_elapsed * time_elapsed / 6;

  for (std::size_t i = 0; i != offset.data.size(); ++i)
  {
    // If new velocity will overshoot assume we would have stopped right on target.
    // TODO: Improve check to see if less jerk would have caused undershoot.
    if ((change_in_position.data[i] / offset.data[i]) > 1.0)
    {
      state->acceleration.data[i] = 0;
      state->velocity.data[i] = 0;
      state->position.data[i] = position_target.data[i];
    }
    else
    {
      state->position.data[i] += change_in_position.data[i];
    }
  }
}

Common::Quaternion GetRotationFromAcceleration(const Common::Vec3& accel)
{
  const auto normalized_accel = accel.Normalized();

  const auto angle = std::acos(normalized_accel.Dot({0, 0, 1}));
  const auto axis = normalized_accel.Cross({0, 0, 1});

  // Check that axis is non-zero to handle perfect up/down orientations.
  return Common::Quaternion::Rotate(angle, axis.LengthSquared() ? axis.Normalized() :
                                                                  Common::Vec3{0, 1, 0});
}

Common::Quaternion GetRotationFromGyroscope(const Common::Vec3& gyro)
{
  const auto length = gyro.Length();
  return (length != 0) ? Common::Quaternion::Rotate(length, gyro / length) :
                         Common::Quaternion::Identity();
}

Common::Matrix33 GetRotationalMatrix(const Common::Vec3& angle)
{
  return Common::Matrix33::RotateZ(angle.z) * Common::Matrix33::RotateY(angle.y) *
         Common::Matrix33::RotateX(angle.x);
}

float GetPitch(const Common::Quaternion& world_rotation)
{
  const auto vec = world_rotation * Common::Vec3{0, 0, 1};
  return std::atan2(vec.y, Common::Vec2(vec.x, vec.z).Length());
}

float GetRoll(const Common::Quaternion& world_rotation)
{
  const auto vec = world_rotation * Common::Vec3{0, 0, 1};
  return std::atan2(vec.x, vec.z);
}

float GetYaw(const Common::Quaternion& world_rotation)
{
  const auto vec = world_rotation.Inverted() * Common::Vec3{0, 1, 0};
  return std::atan2(vec.x, vec.y);
}

}  // namespace WiimoteEmu
