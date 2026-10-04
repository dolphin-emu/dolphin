// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cmath>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include "Common/Config/Config.h"
#include "Common/MathUtil.h"
#include "Core/HW/WiimoteEmu/DesiredWiimoteState.h"
#include "Core/HW/WiimoteEmu/Dynamics.h"

namespace
{
constexpr float TIME_STEP = 1.f / 200;
constexpr float SENSITIVITY = float(MathUtil::TAU / 1800);

// Feed the public Swing bindings in backend-normalized displacement units.
// The count-scale adapter here preserves the prototype's regression trajectories.
void RunSwing(WiimoteEmu::MotionState* state, Common::Vec2 counts, float sensitivity,
              float time_step, bool recenter = false,
              std::optional<Common::Vec2> point_angles = std::nullopt)
{
  ControllerEmu::Force group("Swing");
  group.SetRelativeInput(true);
  EXPECT_NEAR(group.GetSensitivity(), sensitivity * 8, 1e-8);
  const float values[]{-counts.y / 8, counts.y / 8, -counts.x / 8, counts.x / 8};
  for (int i = 0; i != 4; ++i)
    group.SetControlExpression(i, fmt::format("{}", std::max(0.f, values[i])));
  group.SetControlExpression(7, recenter ? "1" : "0");
  WiimoteEmu::EmulateSwing(state, &group, time_step, point_angles);
}

void Move(WiimoteEmu::MotionState* state, Common::Vec2 delta, int steps)
{
  for (int i = 0; i != steps; ++i)
    RunSwing(state, delta, SENSITIVITY, TIME_STEP);
}

void ExpectSameRotation(const Common::Quaternion& a, const Common::Quaternion& b)
{
  // q and -q describe the same rotation.
  EXPECT_NEAR(std::abs(a.data.Dot(b.data)), 1.f, 1e-5f);
}

void SetPose(WiimoteEmu::MotionState* state, Common::Vec2 angles)
{
  state->angle = {angles.y, 0, -angles.x};
}
}  // namespace

TEST(RelativeSwing, Directions)
{
  WiimoteEmu::MotionState down, up, right, left;
  Move(&down, {0, 5}, 20);
  Move(&up, {0, -5}, 20);
  Move(&right, {5, 0}, 20);
  Move(&left, {-5, 0}, 20);
  EXPECT_GT(down.angular_velocity.x, 0);
  EXPECT_LT(down.position.z, 0);
  EXPECT_LT(up.angular_velocity.x, 0);
  EXPECT_GT(up.position.z, 0);
  // Wii Remote X+ points left.
  EXPECT_LT(right.position.x, 0);
  EXPECT_GT(left.position.x, 0);
  EXPECT_FLOAT_EQ(down.position.z, -up.position.z);
  EXPECT_FLOAT_EQ(right.position.x, -left.position.x);
}

TEST(RelativeSwing, SpeedControlsMotionStrength)
{
  WiimoteEmu::MotionState slow, fast;
  Move(&slow, {0, 2.5f}, 10);
  Move(&fast, {0, 20}, 10);
  EXPECT_GT(fast.angular_velocity.x, slow.angular_velocity.x * 4);
  EXPECT_GT(fast.acceleration.Length(), slow.acceleration.Length());
}

TEST(RelativeSwing, StopsWithoutReturningToCenter)
{
  WiimoteEmu::MotionState state;
  Move(&state, {0, -5}, 40);
  Move(&state, {}, 200);
  const auto rotation = WiimoteEmu::GetSwingRotation(state);
  EXPECT_GT(state.position.z, 0.2f);
  EXPECT_FLOAT_EQ(state.angular_velocity.Length(), 0);
  EXPECT_FLOAT_EQ(state.acceleration.Length(), 0);
  Move(&state, {}, 100);
  ExpectSameRotation(WiimoteEmu::GetSwingRotation(state), rotation);
}

TEST(RelativeSwing, BackswingAndDownstroke)
{
  WiimoteEmu::MotionState state;
  Move(&state, {0, -5}, 80);
  Move(&state, {}, 100);
  EXPECT_LT(Common::FromQuaternionToEuler(WiimoteEmu::GetSwingRotation(state)).x, -1.f);
  Move(&state, {0, 25}, 27);
  EXPECT_GT(state.position.z * -1, 0.2f);
  EXPECT_GT(state.angular_velocity.x, 10.f);
  EXPECT_GT(state.acceleration.Length(), float(MathUtil::GRAVITY_ACCELERATION));
}

TEST(RelativeSwing, HorizontalBackswingAndStroke)
{
  WiimoteEmu::MotionState state;
  Move(&state, {-5, 0}, 80);
  Move(&state, {}, 100);
  EXPECT_GT(state.position.x, 0.4f);
  Move(&state, {25, 0}, 27);
  EXPECT_LT(state.position.x, -0.2f);
  EXPECT_LT(state.angular_velocity.z, -10.f);
  EXPECT_GT(state.acceleration.Length(), float(MathUtil::GRAVITY_ACCELERATION));
}

TEST(RelativeSwing, SidewaysSwingWithRaisedArm)
{
  WiimoteEmu::MotionState raised, neutral;
  SetPose(&raised, {0, float(-MathUtil::TAU * 75 / 360)});
  Move(&raised, {5, 0}, 20);
  Move(&neutral, {5, 0}, 20);
  EXPECT_LT(raised.position.x, -0.1f);
  EXPECT_NEAR(raised.position.x, neutral.position.x, 1e-6f);
}

TEST(RelativeSwing, VerticalSwingAfterHorizontalTurn)
{
  WiimoteEmu::MotionState turned, neutral;
  SetPose(&turned, {float(MathUtil::TAU * 75 / 360), 0});
  Move(&turned, {0, 5}, 20);
  Move(&neutral, {0, 5}, 20);
  EXPECT_LT(turned.position.z, -0.03f);
  EXPECT_GT(turned.angular_velocity.x, 0);
}

TEST(RelativeSwing, ExtremeInputCannotInvertThePose)
{
  WiimoteEmu::MotionState state;
  Move(&state, {10000, -10000}, 10000);
  EXPECT_NEAR(WiimoteEmu::GetSwingRotation(state).Norm(), 1.f, 1e-6f);
  EXPECT_LE(state.angular_velocity.Length(), 20.f + 1e-5f);
  EXPECT_LE(std::abs((-state.angle.z)), float(MathUtil::TAU * 85 / 360));
  EXPECT_LE(std::abs(state.angle.x), float(MathUtil::TAU * 85 / 360));
  EXPECT_LE(state.position.Length(), 1.f + 1e-6f);
  EXPECT_TRUE(std::isfinite(state.acceleration.Length()));
}

TEST(RelativeSwing, EqualTravelAtDifferentSamplingRates)
{
  WiimoteEmu::MotionState fast_poll, slow_poll;
  for (int i = 0; i != 100; ++i)
    RunSwing(&fast_poll, {0, 2.5f}, SENSITIVITY, TIME_STEP);
  for (int i = 0; i != 50; ++i)
    RunSwing(&slow_poll, {0, 5}, SENSITIVITY, TIME_STEP * 2);
  Move(&fast_poll, {}, 200);
  Move(&slow_poll, {}, 200);
  ExpectSameRotation(WiimoteEmu::GetSwingRotation(fast_poll),
                     WiimoteEmu::GetSwingRotation(slow_poll));
}

TEST(RelativeSwing, ReportedGyroscopeMatchesMixedRotation)
{
  WiimoteEmu::MotionState state;
  auto integrated = Common::Quaternion::Identity();
  for (int i = 0; i != 500; ++i)
  {
    const Common::Vec2 delta{float(i % 31 - 15), float(i % 23 - 11)};
    RunSwing(&state, delta, SENSITIVITY, TIME_STEP);
    integrated = (integrated * Common::Quaternion::RotateXYZ(state.angular_velocity * TIME_STEP))
                     .Normalized();
  }
  ExpectSameRotation(WiimoteEmu::GetSwingRotation(state), integrated);
}

TEST(RelativeSwing, ConstantSpeedHasCentripetalAcceleration)
{
  WiimoteEmu::MotionState state;
  state.angular_velocity = {1, 0, 0};
  state.input_velocity = {0, 1, 0};
  RunSwing(&state, {0, TIME_STEP / SENSITIVITY}, SENSITIVITY, TIME_STEP);
  const auto local_accel = WiimoteEmu::GetSwingRotation(state).Conjugate() * state.acceleration;
  EXPECT_NEAR(local_accel.x, 0, 1e-6f);
  EXPECT_NEAR(local_accel.y, 0.5f, 1e-6f);
  // Differentiating float quaternion deltas amplifies roundoff by 1 / TIME_STEP.
  EXPECT_NEAR(local_accel.z, 0, 1e-5f);
}

TEST(RelativeSwing, RecenteringReportsRotationInsteadOfTeleporting)
{
  WiimoteEmu::MotionState state;
  SetPose(&state, {0.8f, 1.1f});
  const auto initial = WiimoteEmu::GetSwingRotation(state);
  RunSwing(&state, {}, SENSITIVITY, TIME_STEP, true);
  EXPECT_GT(state.angular_velocity.Length(), 0);
  EXPECT_LT(state.angular_velocity.Length(), 20);
  ExpectSameRotation(WiimoteEmu::GetSwingRotation(state),
                     initial * Common::Quaternion::RotateXYZ(state.angular_velocity * TIME_STEP));
  for (int i = 0; i != 400; ++i)
    RunSwing(&state, {}, SENSITIVITY, TIME_STEP, true);
  ExpectSameRotation(WiimoteEmu::GetSwingRotation(state), Common::Quaternion::Identity());
  EXPECT_NEAR(state.angular_velocity.Length(), 0, 1e-4f);
}

TEST(RelativeSwing, DefaultSensitivityRequiresShortTravel)
{
  WiimoteEmu::MotionState state;
  Move(&state, {0, 5}, 80);
  Move(&state, {}, 200);
  EXPECT_NEAR(state.angle.x, float(MathUtil::TAU * 80 / 360), 1e-4f);
}

TEST(RelativeSwing, InvalidTimeStepDoesNotMove)
{
  WiimoteEmu::MotionState state;
  RunSwing(&state, {100, 100}, SENSITIVITY, 0);
  EXPECT_FLOAT_EQ(state.angular_velocity.Length(), 0);
  EXPECT_FLOAT_EQ(state.position.Length(), 0);
  ExpectSameRotation(WiimoteEmu::GetSwingRotation(state), Common::Quaternion::Identity());
}

TEST(RelativeSwing, ClosedMousePathReturnsToSamePose)
{
  WiimoteEmu::MotionState state;
  for (int i = 0; i != 20; ++i)
  {
    for (const auto delta : {Common::Vec2{5, 0}, {0, 5}, {-5, 0}, {0, -5}})
    {
      Move(&state, delta, 20);
      Move(&state, {}, 100);
    }
  }
  ExpectSameRotation(WiimoteEmu::GetSwingRotation(state), Common::Quaternion::Identity());
  EXPECT_NEAR(Common::Vec2(state.angle.x, state.angle.z).Length(), 0, 1e-4f);
}

TEST(RelativeSwing, DownwardTravelNeverStartsMovingUpward)
{
  WiimoteEmu::MotionState state;
  for (int i = 0; i != 1000; ++i)
  {
    const float previous_z = state.position.z;
    Move(&state, {0, 5}, 1);
    EXPECT_LE(state.position.z, previous_z + 1e-6f);
  }
}

TEST(RelativeSwing, ReversalAtEdgeDoesNotConsumeAccumulatedOverflow)
{
  WiimoteEmu::MotionState state;
  Move(&state, {0, 100}, 1000);
  const float edge = state.angle.x;
  Move(&state, {0, -5}, 1);
  EXPECT_LT(state.angle.x, edge);
  EXPECT_LT(state.angular_velocity.x, 0);
}

TEST(RelativeSwing, DirectionReversalDoesNotKeepMovingTheOldWay)
{
  WiimoteEmu::MotionState state;
  Move(&state, {5, 0}, 20);
  const float previous_x = state.position.x;
  Move(&state, {-5, 0}, 1);
  EXPECT_GT(state.position.x, previous_x);
}

TEST(RelativeSwing, PointingIgnoresRelativeSwingInput)
{
  WiimoteEmu::MotionState state;
  const Common::Vec2 target{0.1f, -0.12f};
  for (int i = 0; i != 200; ++i)
    RunSwing(&state, {1000, -1000}, SENSITIVITY, TIME_STEP, false, target);
  EXPECT_FLOAT_EQ((-state.angle.z), target.x);
  EXPECT_FLOAT_EQ(state.angle.x, target.y);
  EXPECT_FLOAT_EQ(state.acceleration.Length(), 0);
  EXPECT_FLOAT_EQ(state.angular_velocity.Length(), 0);
}

TEST(RelativeSwing, PointingRotationMatchesTheStandardIRTransformation)
{
  WiimoteEmu::MotionState state;
  const Common::Vec2 target{0.1f, -0.12f};
  for (int i = 0; i != 50; ++i)
    RunSwing(&state, {}, SENSITIVITY, TIME_STEP, false, target);
  const auto actual =
      Common::Matrix33::FromQuaternion(WiimoteEmu::GetSwingRotation(state).Conjugate());
  const auto expected = WiimoteEmu::GetRotationalMatrix({-target.y, 0, target.x});
  for (std::size_t i = 0; i != actual.data.size(); ++i)
    EXPECT_NEAR(actual.data[i], expected.data[i], 1e-6f);
}

TEST(RelativeSwing, NormalPointingDoesNotAddAnotherFilter)
{
  WiimoteEmu::MotionState state;
  const Common::Vec2 target{0.005f, -0.005f};
  RunSwing(&state, {}, SENSITIVITY, TIME_STEP, false, target);
  EXPECT_FLOAT_EQ((-state.angle.z), target.x);
  EXPECT_FLOAT_EQ(state.angle.x, target.y);
}

TEST(RelativeSwing, ReturningToPointingReportsConsistentGyroscope)
{
  WiimoteEmu::MotionState state;
  Move(&state, {5, -5}, 70);
  auto integrated = WiimoteEmu::GetSwingRotation(state);
  const Common::Vec2 target{0.1f, 0.12f};
  for (int i = 0; i != 200; ++i)
  {
    RunSwing(&state, {}, SENSITIVITY, TIME_STEP, false, target);
    integrated = (integrated * Common::Quaternion::RotateXYZ(state.angular_velocity * TIME_STEP))
                     .Normalized();
    EXPECT_LE(state.angular_velocity.Length(), 6.f + 1e-3f);
    EXPECT_FLOAT_EQ(state.acceleration.Length(), 0);
  }
  ExpectSameRotation(WiimoteEmu::GetSwingRotation(state), integrated);
  EXPECT_FLOAT_EQ((-state.angle.z), target.x);
  EXPECT_FLOAT_EQ(state.angle.x, target.y);
}

class RelativeSwingPointerTest : public testing::Test
{
protected:
  void SetUp() override { Config::Init(); }
  void TearDown() override { Config::Shutdown(); }
};

TEST_F(RelativeSwingPointerTest, PointerMatchesDefaultDolphinAndStaysStillAtRest)
{
  WiimoteEmu::Wiimote normal(0), mouse(1);
  static_cast<ControllerEmu::Force*>(mouse.GetWiimoteGroup(WiimoteEmu::WiimoteGroup::Swing))
      ->SetRelativeInput(true);
  mouse.GetWiimoteGroup(WiimoteEmu::WiimoteGroup::Swing)->SetControlExpression(6, "0");
  const auto pointer = [](std::string_view group, std::string_view control,
                          ControlState) -> std::optional<ControlState> {
    if (group == WiimoteEmu::Wiimote::IR_GROUP && control == "X")
      return 0.4;
    if (group == WiimoteEmu::Wiimote::IR_GROUP && control == "Y")
      return -0.3;
    return std::nullopt;
  };
  normal.SetInputOverrideFunction(pointer);
  mouse.SetInputOverrideFunction(pointer);
  WiimoteEmu::DesiredWiimoteState normal_input, mouse_input;
  for (int i = 0; i != 200; ++i)
  {
    normal.PrepareInput(&normal_input, WiimoteCommon::HIDWiimote::SensorBarState::Enabled);
    mouse.PrepareInput(&mouse_input, WiimoteCommon::HIDWiimote::SensorBarState::Enabled);
  }
  ASSERT_NE(mouse_input.camera_points, WiimoteEmu::DesiredWiimoteState::DEFAULT_CAMERA);
  EXPECT_EQ(mouse_input.camera_points, normal_input.camera_points);
  EXPECT_EQ(mouse_input.acceleration.value, normal_input.acceleration.value);
  ASSERT_TRUE(mouse_input.motion_plus);
  const auto camera = mouse_input.camera_points;
  const auto gyro = mouse_input.motion_plus->gyro.value;
  for (int i = 0; i != 100; ++i)
  {
    mouse.PrepareInput(&mouse_input, WiimoteCommon::HIDWiimote::SensorBarState::Enabled);
    EXPECT_EQ(mouse_input.camera_points, camera);
    EXPECT_EQ(mouse_input.motion_plus->gyro.value, gyro);
  }
}

TEST_F(RelativeSwingPointerTest, HoldingMotionFreezesThePointerTarget)
{
  WiimoteEmu::Wiimote mouse(0);
  auto* group =
      static_cast<ControllerEmu::Force*>(mouse.GetWiimoteGroup(WiimoteEmu::WiimoteGroup::Swing));
  group->SetRelativeInput(true);
  group->SetControlExpression(6, "0");
  double pointer_x = 0.2;
  mouse.SetInputOverrideFunction([&](std::string_view group_name, std::string_view control,
                                     ControlState) -> std::optional<ControlState> {
    if (group_name == WiimoteEmu::Wiimote::IR_GROUP && control == "X")
      return pointer_x;
    return std::nullopt;
  });
  WiimoteEmu::DesiredWiimoteState input;
  for (int i = 0; i != 100; ++i)
    mouse.PrepareInput(&input, WiimoteCommon::HIDWiimote::SensorBarState::Enabled);
  const auto initial_camera = input.camera_points;
  group->controls[6]->control_ref->SetExpression("1");
  pointer_x = -0.6;
  for (int i = 0; i != 50; ++i)
    mouse.PrepareInput(&input, WiimoteCommon::HIDWiimote::SensorBarState::Enabled);
  EXPECT_EQ(input.camera_points, initial_camera);
  group->controls[6]->control_ref->SetExpression("0");
  for (int i = 0; i != 100; ++i)
    mouse.PrepareInput(&input, WiimoteCommon::HIDWiimote::SensorBarState::Enabled);
  EXPECT_NE(input.camera_points, initial_camera);
}

TEST_F(RelativeSwingPointerTest, StartingWithMotionHeldInitializesTheCamera)
{
  WiimoteEmu::Wiimote mouse(0);
  auto* group =
      static_cast<ControllerEmu::Force*>(mouse.GetWiimoteGroup(WiimoteEmu::WiimoteGroup::Swing));
  group->SetRelativeInput(true);
  group->controls[6]->control_ref->SetExpression("1");
  WiimoteEmu::DesiredWiimoteState input;
  mouse.PrepareInput(&input, WiimoteCommon::HIDWiimote::SensorBarState::Enabled);
  EXPECT_NE(input.camera_points, WiimoteEmu::DesiredWiimoteState::DEFAULT_CAMERA);
  EXPECT_EQ(input.acceleration.value, WiimoteEmu::DesiredWiimoteState::DEFAULT_ACCELERATION.value);
}

TEST(RelativeSwing, AnalogBindingsAreNotLimitedToMouseInputs)
{
  ControllerEmu::Force group("Swing");
  group.SetRelativeInput(true);
  group.SetControlExpression(0, "0.5");
  group.SetControlExpression(3, "0.5");
  WiimoteEmu::MotionState state;
  for (int i = 0; i != 20; ++i)
    WiimoteEmu::EmulateSwing(&state, &group, TIME_STEP);
  EXPECT_LT(state.position.x, 0);
  EXPECT_GT(state.position.z, 0);
  EXPECT_GT(state.angular_velocity.Length(), 0);
  group.SetControlExpression(6, "0");
  EXPECT_FLOAT_EQ(group.GetRelativeState().Length(), 0);
}

TEST(RelativeSwing, ForwardBackwardBindingsTranslateTheArm)
{
  ControllerEmu::Force group("Swing");
  group.SetRelativeInput(true);
  group.SetControlExpression(4, "1");
  WiimoteEmu::MotionState state;
  for (int i = 0; i != 20; ++i)
    WiimoteEmu::EmulateSwing(&state, &group, TIME_STEP);
  EXPECT_LT(state.position.y, 0);
  EXPECT_FLOAT_EQ(state.angular_velocity.Length(), 0);
  const float previous_y = state.position.y;
  group.SetControlExpression(4, "0");
  group.SetControlExpression(5, "1");
  WiimoteEmu::EmulateSwing(&state, &group, TIME_STEP);
  EXPECT_GT(state.position.y, previous_y);
}

TEST(RelativeSwing, ProfilePersistsModeAndSensitivityWithoutClampingDisplacement)
{
  Common::IniFile::Section config;
  config.Set("Swing/Relative Input", true);
  config.Set("Swing/Motion Sensitivity", 200);
  config.Set("Swing/Horizontal Sensitivity", 50);
  config.Set("Swing/Vertical Sensitivity", 150);
  ControllerEmu::Force group("Swing");
  static_cast<ControllerEmu::ControlGroup&>(group).LoadConfig(&config, "");
  group.SetControlExpression(3, "2");
  group.SetControlExpression(0, "3");
  ASSERT_TRUE(group.IsRelativeInput());
  EXPECT_FLOAT_EQ(group.GetRelativeState().x, 1);
  EXPECT_FLOAT_EQ(group.GetRelativeState().y, 4.5);
  EXPECT_NEAR(group.GetSensitivity(), SENSITIVITY * 16, 1e-8);
  Common::IniFile::Section saved;
  static_cast<ControllerEmu::ControlGroup&>(group).SaveConfig(&saved, "");
  ControllerEmu::Force restored("Swing");
  static_cast<ControllerEmu::ControlGroup&>(restored).LoadConfig(&saved, "");
  EXPECT_TRUE(restored.IsRelativeInput());
  EXPECT_FLOAT_EQ(restored.GetRelativeState().x, 1);
  EXPECT_FLOAT_EQ(restored.GetRelativeState().y, 4.5);
  EXPECT_DOUBLE_EQ(restored.GetSensitivity(), group.GetSensitivity());
}

TEST(Swing, AbsoluteInputStillReturnsToNeutralByDefault)
{
  ControllerEmu::Force group("Swing");
  ASSERT_FALSE(group.IsRelativeInput());
  group.SetControlExpression(0, "1");
  WiimoteEmu::MotionState state;
  for (int i = 0; i != 200; ++i)
    WiimoteEmu::EmulateSwing(&state, &group, TIME_STEP);
  EXPECT_GT(state.position.z, 0.4f);
  group.SetControlExpression(0, "0");
  for (int i = 0; i != 1000; ++i)
    WiimoteEmu::EmulateSwing(&state, &group, TIME_STEP);
  EXPECT_NEAR(state.position.Length(), 0, 1e-5f);
  EXPECT_NEAR(state.angle.Length(), 0, 1e-5f);
}
