// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cmath>

#include <gtest/gtest.h>

#include "Common/Config/Config.h"
#include "Common/MathUtil.h"
#include "Core/HW/WiimoteEmu/DesiredWiimoteState.h"
#include "Core/HW/WiimoteEmu/Dynamics.h"

namespace
{
constexpr float TIME_STEP = 1.f / 200;
constexpr float SENSITIVITY = float(MathUtil::TAU / 1800);

void Move(WiimoteEmu::MouseMotionState* state, Common::Vec2 delta, int steps)
{
  for (int i = 0; i != steps; ++i)
    WiimoteEmu::EmulateMouseMotion(state, delta, SENSITIVITY, TIME_STEP);
}

void ExpectSameRotation(const Common::Quaternion& a, const Common::Quaternion& b)
{
  // q and -q describe the same rotation.
  EXPECT_NEAR(std::abs(a.data.Dot(b.data)), 1.f, 1e-5f);
}

void SetPose(WiimoteEmu::MouseMotionState* state, Common::Vec2 angles)
{
  state->angles = angles;
  state->rotation = Common::Quaternion::RotateX(angles.y) * Common::Quaternion::RotateZ(-angles.x);
}
}  // namespace

TEST(MouseMotion, Directions)
{
  WiimoteEmu::MouseMotionState down, up, right, left;
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

TEST(MouseMotion, SpeedControlsMotionStrength)
{
  WiimoteEmu::MouseMotionState slow, fast;
  Move(&slow, {0, 2.5f}, 10);
  Move(&fast, {0, 20}, 10);
  EXPECT_GT(fast.angular_velocity.x, slow.angular_velocity.x * 4);
  EXPECT_GT(fast.acceleration.Length(), slow.acceleration.Length());
}

TEST(MouseMotion, StopsWithoutReturningToCenter)
{
  WiimoteEmu::MouseMotionState state;
  Move(&state, {0, -5}, 40);
  Move(&state, {}, 200);
  const auto rotation = state.rotation;
  EXPECT_GT(state.position.z, 0.2f);
  EXPECT_FLOAT_EQ(state.angular_velocity.Length(), 0);
  EXPECT_FLOAT_EQ(state.acceleration.Length(), 0);
  Move(&state, {}, 100);
  ExpectSameRotation(state.rotation, rotation);
}

TEST(MouseMotion, BackswingAndDownstroke)
{
  WiimoteEmu::MouseMotionState state;
  Move(&state, {0, -5}, 80);
  Move(&state, {}, 100);
  EXPECT_LT(Common::FromQuaternionToEuler(state.rotation).x, -1.f);
  Move(&state, {0, 25}, 27);
  EXPECT_GT(state.position.z * -1, 0.2f);
  EXPECT_GT(state.angular_velocity.x, 10.f);
  EXPECT_GT(state.acceleration.Length(), float(MathUtil::GRAVITY_ACCELERATION));
}

TEST(MouseMotion, HorizontalBackswingAndStroke)
{
  WiimoteEmu::MouseMotionState state;
  Move(&state, {-5, 0}, 80);
  Move(&state, {}, 100);
  EXPECT_GT(state.position.x, 0.4f);
  Move(&state, {25, 0}, 27);
  EXPECT_LT(state.position.x, -0.2f);
  EXPECT_LT(state.angular_velocity.z, -10.f);
  EXPECT_GT(state.acceleration.Length(), float(MathUtil::GRAVITY_ACCELERATION));
}

TEST(MouseMotion, SidewaysSwingWithRaisedArm)
{
  WiimoteEmu::MouseMotionState raised, neutral;
  SetPose(&raised, {0, float(-MathUtil::TAU * 75 / 360)});
  Move(&raised, {5, 0}, 20);
  Move(&neutral, {5, 0}, 20);
  EXPECT_LT(raised.position.x, -0.1f);
  EXPECT_NEAR(raised.position.x, neutral.position.x, 1e-6f);
}

TEST(MouseMotion, VerticalSwingAfterHorizontalTurn)
{
  WiimoteEmu::MouseMotionState turned, neutral;
  SetPose(&turned, {float(MathUtil::TAU * 75 / 360), 0});
  Move(&turned, {0, 5}, 20);
  Move(&neutral, {0, 5}, 20);
  EXPECT_LT(turned.position.z, -0.03f);
  EXPECT_GT(turned.angular_velocity.x, 0);
}

TEST(MouseMotion, ExtremeInputCannotInvertThePose)
{
  WiimoteEmu::MouseMotionState state;
  Move(&state, {10000, -10000}, 10000);
  EXPECT_NEAR(state.rotation.Norm(), 1.f, 1e-6f);
  EXPECT_LE(state.angular_velocity.Length(), 20.f + 1e-5f);
  EXPECT_LE(std::abs(state.angles.x), float(MathUtil::TAU * 85 / 360));
  EXPECT_LE(std::abs(state.angles.y), float(MathUtil::TAU * 85 / 360));
  EXPECT_LE(state.position.Length(), 1.f + 1e-6f);
  EXPECT_TRUE(std::isfinite(state.acceleration.Length()));
}

TEST(MouseMotion, EqualTravelAtDifferentSamplingRates)
{
  WiimoteEmu::MouseMotionState fast_poll, slow_poll;
  for (int i = 0; i != 100; ++i)
    WiimoteEmu::EmulateMouseMotion(&fast_poll, {0, 2.5f}, SENSITIVITY, TIME_STEP);
  for (int i = 0; i != 50; ++i)
    WiimoteEmu::EmulateMouseMotion(&slow_poll, {0, 5}, SENSITIVITY, TIME_STEP * 2);
  Move(&fast_poll, {}, 200);
  Move(&slow_poll, {}, 200);
  ExpectSameRotation(fast_poll.rotation, slow_poll.rotation);
}

TEST(MouseMotion, ReportedGyroscopeMatchesMixedRotation)
{
  WiimoteEmu::MouseMotionState state;
  auto integrated = Common::Quaternion::Identity();
  for (int i = 0; i != 500; ++i)
  {
    const Common::Vec2 delta{float(i % 31 - 15), float(i % 23 - 11)};
    WiimoteEmu::EmulateMouseMotion(&state, delta, SENSITIVITY, TIME_STEP);
    integrated = (integrated * Common::Quaternion::RotateXYZ(state.angular_velocity * TIME_STEP))
                     .Normalized();
  }
  ExpectSameRotation(state.rotation, integrated);
}

TEST(MouseMotion, ConstantSpeedHasCentripetalAcceleration)
{
  WiimoteEmu::MouseMotionState state;
  state.angular_velocity = {1, 0, 0};
  state.angle_velocity = {0, 1};
  WiimoteEmu::EmulateMouseMotion(&state, {0, TIME_STEP / SENSITIVITY}, SENSITIVITY, TIME_STEP);
  const auto local_accel = state.rotation.Conjugate() * state.acceleration;
  EXPECT_NEAR(local_accel.x, 0, 1e-6f);
  EXPECT_NEAR(local_accel.y, 0.5f, 1e-6f);
  // Differentiating float quaternion deltas amplifies roundoff by 1 / TIME_STEP.
  EXPECT_NEAR(local_accel.z, 0, 1e-5f);
}

TEST(MouseMotion, RecenteringReportsRotationInsteadOfTeleporting)
{
  WiimoteEmu::MouseMotionState state;
  SetPose(&state, {0.8f, 1.1f});
  const auto initial = state.rotation;
  WiimoteEmu::EmulateMouseMotion(&state, {}, SENSITIVITY, TIME_STEP, true);
  EXPECT_GT(state.angular_velocity.Length(), 0);
  EXPECT_LT(state.angular_velocity.Length(), 20);
  ExpectSameRotation(state.rotation,
                     initial * Common::Quaternion::RotateXYZ(state.angular_velocity * TIME_STEP));
  for (int i = 0; i != 400; ++i)
    WiimoteEmu::EmulateMouseMotion(&state, {}, SENSITIVITY, TIME_STEP, true);
  ExpectSameRotation(state.rotation, Common::Quaternion::Identity());
  EXPECT_NEAR(state.angular_velocity.Length(), 0, 1e-4f);
}

TEST(MouseMotion, DefaultSensitivityRequiresShortTravel)
{
  WiimoteEmu::MouseMotionState state;
  Move(&state, {0, 5}, 80);
  Move(&state, {}, 200);
  EXPECT_NEAR(state.angles.y, float(MathUtil::TAU * 80 / 360), 1e-4f);
}

TEST(MouseMotion, InvalidTimeStepDoesNotMove)
{
  WiimoteEmu::MouseMotionState state;
  WiimoteEmu::EmulateMouseMotion(&state, {100, 100}, SENSITIVITY, 0);
  EXPECT_FLOAT_EQ(state.angular_velocity.Length(), 0);
  EXPECT_FLOAT_EQ(state.position.Length(), 0);
  ExpectSameRotation(state.rotation, Common::Quaternion::Identity());
}

TEST(MouseMotion, ClosedMousePathReturnsToSamePose)
{
  WiimoteEmu::MouseMotionState state;
  for (int i = 0; i != 20; ++i)
  {
    for (const auto delta : {Common::Vec2{5, 0}, {0, 5}, {-5, 0}, {0, -5}})
    {
      Move(&state, delta, 20);
      Move(&state, {}, 100);
    }
  }
  ExpectSameRotation(state.rotation, Common::Quaternion::Identity());
  EXPECT_NEAR(state.angles.Length(), 0, 1e-4f);
}

TEST(MouseMotion, DownwardTravelNeverStartsMovingUpward)
{
  WiimoteEmu::MouseMotionState state;
  for (int i = 0; i != 1000; ++i)
  {
    const float previous_z = state.position.z;
    Move(&state, {0, 5}, 1);
    EXPECT_LE(state.position.z, previous_z + 1e-6f);
  }
}

TEST(MouseMotion, ReversalAtEdgeDoesNotConsumeAccumulatedOverflow)
{
  WiimoteEmu::MouseMotionState state;
  Move(&state, {0, 100}, 1000);
  const float edge = state.angles.y;
  Move(&state, {0, -5}, 1);
  EXPECT_LT(state.angles.y, edge);
  EXPECT_LT(state.angular_velocity.x, 0);
}

TEST(MouseMotion, DirectionReversalDoesNotKeepMovingTheOldWay)
{
  WiimoteEmu::MouseMotionState state;
  Move(&state, {5, 0}, 20);
  const float previous_x = state.position.x;
  Move(&state, {-5, 0}, 1);
  EXPECT_GT(state.position.x, previous_x);
}

TEST(MouseMotion, PointingIgnoresRelativeSwingInput)
{
  WiimoteEmu::MouseMotionState state;
  const Common::Vec2 target{0.1f, -0.12f};
  for (int i = 0; i != 200; ++i)
    WiimoteEmu::EmulateMouseMotion(&state, {1000, -1000}, SENSITIVITY, TIME_STEP, false, target);
  EXPECT_FLOAT_EQ(state.angles.x, target.x);
  EXPECT_FLOAT_EQ(state.angles.y, target.y);
  EXPECT_FLOAT_EQ(state.acceleration.Length(), 0);
  EXPECT_FLOAT_EQ(state.angular_velocity.Length(), 0);
}

TEST(MouseMotion, PointingRotationMatchesTheStandardIRTransformation)
{
  WiimoteEmu::MouseMotionState state;
  const Common::Vec2 target{0.1f, -0.12f};
  for (int i = 0; i != 50; ++i)
    WiimoteEmu::EmulateMouseMotion(&state, {}, SENSITIVITY, TIME_STEP, false, target);
  const auto actual = Common::Matrix33::FromQuaternion(state.rotation.Conjugate());
  const auto expected = WiimoteEmu::GetRotationalMatrix({-target.y, 0, target.x});
  for (std::size_t i = 0; i != actual.data.size(); ++i)
    EXPECT_NEAR(actual.data[i], expected.data[i], 1e-6f);
}

TEST(MouseMotion, NormalPointingDoesNotAddAnotherFilter)
{
  WiimoteEmu::MouseMotionState state;
  const Common::Vec2 target{0.005f, -0.005f};
  WiimoteEmu::EmulateMouseMotion(&state, {}, SENSITIVITY, TIME_STEP, false, target);
  EXPECT_FLOAT_EQ(state.angles.x, target.x);
  EXPECT_FLOAT_EQ(state.angles.y, target.y);
}

TEST(MouseMotion, ReturningToPointingReportsConsistentGyroscope)
{
  WiimoteEmu::MouseMotionState state;
  Move(&state, {5, -5}, 70);
  auto integrated = state.rotation;
  const Common::Vec2 target{0.1f, 0.12f};
  for (int i = 0; i != 200; ++i)
  {
    WiimoteEmu::EmulateMouseMotion(&state, {}, SENSITIVITY, TIME_STEP, false, target);
    integrated = (integrated * Common::Quaternion::RotateXYZ(state.angular_velocity * TIME_STEP))
                     .Normalized();
    EXPECT_LE(state.angular_velocity.Length(), 6.f + 1e-3f);
    EXPECT_FLOAT_EQ(state.acceleration.Length(), 0);
  }
  ExpectSameRotation(state.rotation, integrated);
  EXPECT_FLOAT_EQ(state.angles.x, target.x);
  EXPECT_FLOAT_EQ(state.angles.y, target.y);
}

class MouseMotionPointerTest : public testing::Test
{
protected:
  void SetUp() override { Config::Init(); }
  void TearDown() override { Config::Shutdown(); }
};

TEST_F(MouseMotionPointerTest, PointerMatchesDefaultDolphinAndStaysStillAtRest)
{
  WiimoteEmu::Wiimote normal(0), mouse(1);
  mouse.GetWiimoteGroup(WiimoteEmu::WiimoteGroup::MouseMotion)->enabled.SetValue(true);
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

TEST_F(MouseMotionPointerTest, HoldingMotionFreezesThePointerTarget)
{
  WiimoteEmu::Wiimote mouse(0);
  auto* group = mouse.GetWiimoteGroup(WiimoteEmu::WiimoteGroup::MouseMotion);
  group->enabled.SetValue(true);
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
  group->controls[0]->control_ref->SetExpression("1");
  pointer_x = -0.6;
  for (int i = 0; i != 50; ++i)
    mouse.PrepareInput(&input, WiimoteCommon::HIDWiimote::SensorBarState::Enabled);
  EXPECT_EQ(input.camera_points, initial_camera);
  group->controls[0]->control_ref->SetExpression("0");
  for (int i = 0; i != 100; ++i)
    mouse.PrepareInput(&input, WiimoteCommon::HIDWiimote::SensorBarState::Enabled);
  EXPECT_NE(input.camera_points, initial_camera);
}

TEST_F(MouseMotionPointerTest, StartingWithMotionHeldInitializesTheCamera)
{
  WiimoteEmu::Wiimote mouse(0);
  auto* group = mouse.GetWiimoteGroup(WiimoteEmu::WiimoteGroup::MouseMotion);
  group->enabled.SetValue(true);
  group->controls[0]->control_ref->SetExpression("1");
  WiimoteEmu::DesiredWiimoteState input;
  mouse.PrepareInput(&input, WiimoteCommon::HIDWiimote::SensorBarState::Enabled);
  EXPECT_NE(input.camera_points, WiimoteEmu::DesiredWiimoteState::DEFAULT_CAMERA);
  EXPECT_EQ(input.acceleration.value, WiimoteEmu::DesiredWiimoteState::DEFAULT_ACCELERATION.value);
}
