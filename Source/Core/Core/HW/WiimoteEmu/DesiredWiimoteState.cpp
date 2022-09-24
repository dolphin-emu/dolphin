// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstring>
#include <optional>
#include <type_traits>
#include <variant>

#include "Common/CommonTypes.h"

#include "Core/HW/WiimoteEmu/DesiredWiimoteState.h"
#include "Core/HW/WiimoteEmu/Extension/Classic.h"
#include "Core/HW/WiimoteEmu/Extension/DrawsomeTablet.h"
#include "Core/HW/WiimoteEmu/Extension/Drums.h"
#include "Core/HW/WiimoteEmu/Extension/Guitar.h"
#include "Core/HW/WiimoteEmu/Extension/Nunchuk.h"
#include "Core/HW/WiimoteEmu/Extension/Shinkansen.h"
#include "Core/HW/WiimoteEmu/Extension/TaTaCon.h"
#include "Core/HW/WiimoteEmu/Extension/Turntable.h"
#include "Core/HW/WiimoteEmu/Extension/UDrawTablet.h"
#include "Core/HW/WiimoteEmu/MotionPlus.h"

namespace WiimoteEmu
{
SerializedWiimoteState SerializeDesiredState(const DesiredWiimoteState& state)
{
  // If no motion or extension data is present, we can serialize into a much smaller representation,
  // which reduces data sent over the network.
  const bool is_buttons_only =
      (state.acceleration == DesiredWiimoteState::DEFAULT_ACCELERATION &&
       state.camera_points == DesiredWiimoteState::DEFAULT_CAMERA &&
       !state.motion_plus.has_value() && state.extension.data.index() == ExtensionNumber::NONE);

  SerializedWiimoteState s;
  s.length = 0;
  s.data[s.length++] =
      u8((state.buttons.a) | (state.buttons.b << 1) | (state.buttons.plus << 2) |
         (state.buttons.minus << 3) | (state.buttons.one << 4) | (state.buttons.two << 5) |
         (state.buttons.home << 6) | ((is_buttons_only ? 0 : 1) << 7));
  const u8 dpad = u8((state.buttons.up) | (state.buttons.down << 1) | (state.buttons.left << 2) |
                     (state.buttons.right << 3));
  if (is_buttons_only)
  {
    // There's four unused bits here, which could be used to further optimize for common situations.
    s.data[s.length++] = dpad;
    return s;
  }

  const u16 accel_x = state.acceleration.value.x;             // 10 bits
  const u16 accel_y = state.acceleration.value.y;             // 10 bits
  const u16 accel_z = state.acceleration.value.z;             // 10 bits
  const u16 camera_p0_x = state.camera_points[0].position.x;  // 10 bits
  const u16 camera_p0_y = state.camera_points[0].position.y;  // 10 bits
  const u16 camera_p1_x = state.camera_points[1].position.x;  // 10 bits
  const u16 camera_p1_y = state.camera_points[1].position.y;  // 10 bits
  const u8 camera_p0_size = state.camera_points[0].size;      // 4 bits
  const u8 camera_p1_size = state.camera_points[1].size;      // 4 bits
  static_assert(std::variant_size_v<DesiredExtensionState::ExtensionData> < (1 << 5));
  const u8 extension = u8(state.extension.data.index());  // maximum of 5 bits
  const u8 motion_plus = state.motion_plus.has_value() ? 1 : 0;
  s.data[s.length++] = u8(dpad | (camera_p0_size << 4));
  s.data[s.length++] = u8(camera_p0_x);
  s.data[s.length++] = u8(camera_p0_y);
  s.data[s.length++] = u8(camera_p1_x);
  s.data[s.length++] = u8(camera_p1_y);
  s.data[s.length++] = u8(((camera_p0_x >> 8) & 3) | (((camera_p0_y >> 8) & 3) << 2) |
                          (((camera_p1_x >> 8) & 3) << 4) | (((camera_p1_y >> 8) & 3) << 6));
  s.data[s.length++] =
      u8(((accel_x >> 8) & 3) | (((accel_y >> 8) & 3) << 2) | (camera_p1_size << 4));
  s.data[s.length++] = u8(accel_x);
  s.data[s.length++] = u8(accel_y);
  s.data[s.length++] = u8(accel_z);
  s.data[s.length++] = u8(((accel_z >> 8) & 3) | (motion_plus << 2) | (extension << 3));

  if (motion_plus)
  {
    const u16 pitch_slow = state.motion_plus->is_slow.x ? 1 : 0;
    const u16 roll_slow = state.motion_plus->is_slow.y ? 1 : 0;
    const u16 yaw_slow = state.motion_plus->is_slow.z ? 1 : 0;
    const u16 pitch_value = state.motion_plus->gyro.value.x;  // 14 bits
    const u16 roll_value = state.motion_plus->gyro.value.y;   // 14 bits
    const u16 yaw_value = state.motion_plus->gyro.value.z;    // 14 bits
    s.data[s.length++] = u8(pitch_value);
    s.data[s.length++] = u8((pitch_value >> 8) | (pitch_slow << 7));
    s.data[s.length++] = u8(roll_value);
    s.data[s.length++] = u8((roll_value >> 8) | (roll_slow << 7));
    s.data[s.length++] = u8(yaw_value);
    s.data[s.length++] = u8((yaw_value >> 8) | (yaw_slow << 7));
  }

  if (extension)
  {
    std::visit(
        [&s](const auto& arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (!std::is_same_v<std::monostate, T>)
          {
            static_assert(sizeof(arg) <= 6);
            static_assert(std::is_trivially_copyable_v<T>);
            std::memcpy(&s.data[s.length], &arg, sizeof(arg));
            s.length += sizeof(arg);
          }
        },
        state.extension.data);
  }

  return s;
}

template <typename T>
static bool DeserializeExtensionState(DesiredWiimoteState* state,
                                      const SerializedWiimoteState& serialized, size_t offset)
{
  if (serialized.length < offset + sizeof(T))
    return false;
  auto& e = state->extension.data.emplace<T>();
  static_assert(std::is_trivially_copyable_v<T>);
  std::memcpy(&e, &serialized.data[offset], sizeof(T));
  return true;
}

bool DeserializeDesiredState(DesiredWiimoteState* state, const SerializedWiimoteState& serialized)
{
  // clear state
  state->buttons.hex = 0;
  state->acceleration = DesiredWiimoteState::DEFAULT_ACCELERATION;
  state->camera_points = DesiredWiimoteState::DEFAULT_CAMERA;
  state->motion_plus = std::nullopt;
  state->extension.data = std::monostate();

  if (serialized.length < 2)
  {
    // can't be valid
    return false;
  }

  const auto& d = serialized.data;
  state->buttons.a = d[0] & 1;
  state->buttons.b = (d[0] >> 1) & 1;
  state->buttons.plus = (d[0] >> 2) & 1;
  state->buttons.minus = (d[0] >> 3) & 1;
  state->buttons.one = (d[0] >> 4) & 1;
  state->buttons.two = (d[0] >> 5) & 1;
  state->buttons.home = (d[0] >> 6) & 1;
  const u8 has_motion_or_extension_data = (d[0] >> 7) & 1;
  state->buttons.up = d[1] & 1;
  state->buttons.down = (d[1] >> 1) & 1;
  state->buttons.left = (d[1] >> 2) & 1;
  state->buttons.right = (d[1] >> 3) & 1;

  if (!has_motion_or_extension_data)
  {
    // is button-only state, we're done
    return true;
  }

  if (serialized.length < 12)
  {
    // if it's not a button-only state it needs to have at least 12 bytes for the basic Wiimote data
    return false;
  }

  state->camera_points[0].size = d[1] >> 4;
  state->camera_points[0].position.x = d[2] | ((d[6] & 3) << 8);
  state->camera_points[0].position.y = d[3] | (((d[6] >> 2) & 3) << 8);
  state->camera_points[1].position.x = d[4] | (((d[6] >> 4) & 3) << 8);
  state->camera_points[1].position.y = d[5] | (((d[6] >> 6) & 3) << 8);
  state->camera_points[1].size = d[7] >> 4;
  state->acceleration.value.x = d[8] | ((d[7] & 3) << 8);
  state->acceleration.value.y = d[9] | (((d[7] >> 2) & 3) << 8);
  state->acceleration.value.z = d[10] | ((d[11] & 3) << 8);
  const u8 has_motion_plus = (d[11] >> 2) & 1;
  const u8 extension = d[11] >> 3;

  if (has_motion_plus && serialized.length < 18)
  {
    // can't hold motion plus data
    return false;
  }

  if (has_motion_plus)
  {
    const u16 pitch_value = d[12] | ((d[13] & 0x3f) << 8);
    const u16 roll_value = d[14] | ((d[15] & 0x3f) << 8);
    const u16 yaw_value = d[16] | ((d[17] & 0x3f) << 8);
    const bool pitch_slow = (d[13] & 0x80) != 0;
    const bool roll_slow = (d[15] & 0x80) != 0;
    const bool yaw_slow = (d[17] & 0x80) != 0;
    state->motion_plus = MotionPlus::DataFormat::Data{
        MotionPlus::DataFormat::GyroRawValue{
            MotionPlus::DataFormat::GyroType(pitch_value, roll_value, yaw_value)},
        MotionPlus::DataFormat::SlowType(pitch_slow, roll_slow, yaw_slow)};
  }

  const size_t offset = has_motion_plus ? 18 : 12;
  switch (extension)
  {
  case ExtensionNumber::NONE:
    return true;
  case ExtensionNumber::NUNCHUK:
    return DeserializeExtensionState<Nunchuk::DataFormat>(state, serialized, offset);
  case ExtensionNumber::CLASSIC:
    return DeserializeExtensionState<Classic::DataFormat>(state, serialized, offset);
  case ExtensionNumber::GUITAR:
    return DeserializeExtensionState<Guitar::DataFormat>(state, serialized, offset);
  case ExtensionNumber::DRUMS:
    return DeserializeExtensionState<Drums::DesiredState>(state, serialized, offset);
  case ExtensionNumber::TURNTABLE:
    return DeserializeExtensionState<Turntable::DataFormat>(state, serialized, offset);
  case ExtensionNumber::UDRAW_TABLET:
    return DeserializeExtensionState<UDrawTablet::DataFormat>(state, serialized, offset);
  case ExtensionNumber::DRAWSOME_TABLET:
    return DeserializeExtensionState<DrawsomeTablet::DataFormat>(state, serialized, offset);
  case ExtensionNumber::TATACON:
    return DeserializeExtensionState<TaTaCon::DataFormat>(state, serialized, offset);
  case ExtensionNumber::SHINKANSEN:
    return DeserializeExtensionState<Shinkansen::DesiredState>(state, serialized, offset);
  default:
    break;
  }

  return false;
}
}  // namespace WiimoteEmu
