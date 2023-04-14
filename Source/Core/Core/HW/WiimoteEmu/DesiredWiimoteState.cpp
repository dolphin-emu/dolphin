// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>
#include <optional>
#include <type_traits>
#include <variant>

#include "Common/BitUtils.h"
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
  const u8 has_buttons = (state.buttons.hex & WiimoteCommon::ButtonData::BUTTON_MASK) != 0 ? 1 : 0;
  const u8 has_accel = state.acceleration != DesiredWiimoteState::DEFAULT_ACCELERATION ? 1 : 0;
  const u8 has_camera = state.camera_points != DesiredWiimoteState::DEFAULT_CAMERA ? 1 : 0;
  const u8 has_motion_plus = state.motion_plus.has_value() ? 1 : 0;

  // Right now we support < 16 extensions so the info which extension is in use fits into 4 bits.
  // This allows 'empty' packets to be a single byte, which is very nice for reducing bandwidth.
  // If we ever support 16 or more we have to redesign this a bit; ideally use a variable-length
  // encoding so that typical extensions (None, Nunchuk, Classic Controller) still fit into the
  // initial 4 bits.
  static_assert(std::variant_size_v<DesiredExtensionState::ExtensionData> <= (1 << 4));
  const u8 extension = u8(state.extension.data.index());

  SerializedWiimoteState s;
  s.length = 0;
  s.data[s.length++] = u8(has_buttons | (has_accel << 1) | (has_camera << 2) |
                          (has_motion_plus << 3) | (extension << 4));

  if (has_buttons)
  {
    const u8 buttons = u8((state.buttons.a) | (state.buttons.b << 1) | (state.buttons.plus << 2) |
                          (state.buttons.minus << 3) | (state.buttons.one << 4) |
                          (state.buttons.two << 5) | (state.buttons.home << 6));
    const u8 dpad = u8((state.buttons.up) | (state.buttons.down << 1) | (state.buttons.left << 2) |
                       (state.buttons.right << 3));
    s.data[s.length++] = buttons;
    s.data[s.length++] = dpad;
  }

  if (has_accel)
  {
    const u16 accel_x = state.acceleration.value.x;  // 10 bits
    const u16 accel_y = state.acceleration.value.y;  // 9 bits (ignore lowest bit)
    const u16 accel_z = state.acceleration.value.z;  // 9 bits (ignore lowest bit)
    const u8 accel_x_high = u8(accel_x >> 2);
    const u8 accel_y_high = u8(accel_y >> 2);
    const u8 accel_z_high = u8(accel_z >> 2);
    const u8 accel_low = u8((accel_x & 0b11) | (Common::ExtractBit<1>(accel_y) << 2) |
                            (Common::ExtractBit<1>(accel_z) << 3));

    if (has_buttons)
    {
      // can use the high bits of the dpad field from buttons
      s.data[s.length - 1] |= u8(accel_low << 4);
    }
    else
    {
      s.data[s.length++] = u8(accel_low << 4);
    }

    s.data[s.length++] = accel_x_high;
    s.data[s.length++] = accel_y_high;
    s.data[s.length++] = accel_z_high;
  }

  if (has_camera)
  {
    for (size_t i = 0; i < 2; ++i)
    {
      const u16 camera_x = state.camera_points[i].position.x;  // 10 bits
      const u16 camera_y = state.camera_points[i].position.y;  // 10 bits
      const u8 camera_size = state.camera_points[i].size;      // 4 bits
      s.data[s.length++] = u8((camera_x & 0b11) | ((camera_y & 0b11) << 2) | (camera_size << 4));
      s.data[s.length++] = u8(camera_x >> 2);
      s.data[s.length++] = u8(camera_y >> 2);
    }
  }

  if (has_motion_plus)
  {
    const u16 pitch_slow = state.motion_plus->is_slow.x ? 1 : 0;
    const u16 roll_slow = state.motion_plus->is_slow.y ? 1 : 0;
    const u16 yaw_slow = state.motion_plus->is_slow.z ? 1 : 0;
    const u16 pitch_value = state.motion_plus->gyro.value.x;  // 14 bits
    const u16 roll_value = state.motion_plus->gyro.value.y;   // 14 bits
    const u16 yaw_value = state.motion_plus->gyro.value.z;    // 14 bits
    s.data[s.length++] = u8(pitch_value);
    s.data[s.length++] = u8(((pitch_value >> 8) & 0x3f) | (pitch_slow << 7));
    s.data[s.length++] = u8(roll_value);
    s.data[s.length++] = u8(((roll_value >> 8) & 0x3f) | (roll_slow << 7));
    s.data[s.length++] = u8(yaw_value);
    s.data[s.length++] = u8(((yaw_value >> 8) & 0x3f) | (yaw_slow << 7));
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
  std::memcpy(static_cast<void*>(&e), static_cast<const void*>(&serialized.data[offset]),
              sizeof(T));
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

  if (serialized.length < 1)
  {
    // can't be valid
    return false;
  }

  const auto& d = serialized.data;
  const u8 has_buttons = d[0] & 1;
  const u8 has_accel = (d[0] >> 1) & 1;
  const u8 has_camera = (d[0] >> 2) & 1;
  const u8 has_motion_plus = (d[0] >> 3) & 1;
  const u8 extension = (d[0] >> 4);

  if (extension >= ExtensionNumber::MAX)
  {
    // invalid extension
    return false;
  }

  const size_t expected_size = [&]() {
    size_t s = 1;
    if (has_buttons && has_accel)
      s += 5;
    else if (has_buttons)
      s += 2;
    else if (has_accel)
      s += 4;
    if (has_camera)
      s += 6;
    if (has_motion_plus)
      s += 6;
    switch (extension)
    {
    case ExtensionNumber::NONE:
      break;
    case ExtensionNumber::NUNCHUK:
      s += sizeof(Nunchuk::DataFormat);
      break;
    case ExtensionNumber::CLASSIC:
      s += sizeof(Classic::DataFormat);
      break;
    case ExtensionNumber::GUITAR:
      s += sizeof(Guitar::DataFormat);
      break;
    case ExtensionNumber::DRUMS:
      s += sizeof(Drums::DesiredState);
      break;
    case ExtensionNumber::TURNTABLE:
      s += sizeof(Turntable::DataFormat);
      break;
    case ExtensionNumber::UDRAW_TABLET:
      s += sizeof(UDrawTablet::DataFormat);
      break;
    case ExtensionNumber::DRAWSOME_TABLET:
      s += sizeof(DrawsomeTablet::DataFormat);
      break;
    case ExtensionNumber::TATACON:
      s += sizeof(TaTaCon::DataFormat);
      break;
    case ExtensionNumber::SHINKANSEN:
      s += sizeof(Shinkansen::DesiredState);
      break;
    default:
      break;
    }
    return s;
  }();

  if (serialized.length != expected_size)
  {
    // invalid length
    return false;
  }

  size_t pos = 1;

  if (has_buttons)
  {
    state->buttons.a = d[pos] & 1;
    state->buttons.b = (d[pos] >> 1) & 1;
    state->buttons.plus = (d[pos] >> 2) & 1;
    state->buttons.minus = (d[pos] >> 3) & 1;
    state->buttons.one = (d[pos] >> 4) & 1;
    state->buttons.two = (d[pos] >> 5) & 1;
    state->buttons.home = (d[pos] >> 6) & 1;
    state->buttons.up = d[pos + 1] & 1;
    state->buttons.down = (d[pos + 1] >> 1) & 1;
    state->buttons.left = (d[pos + 1] >> 2) & 1;
    state->buttons.right = (d[pos + 1] >> 3) & 1;
    pos += 2;
  }

  if (has_accel)
  {
    if (has_buttons)
      pos -= 1;
    const u8 accel_low = d[pos] >> 4;
    const u8 accel_x_high = d[pos + 1];
    const u8 accel_y_high = d[pos + 2];
    const u8 accel_z_high = d[pos + 3];
    state->acceleration.value.x = (accel_x_high << 2) | (accel_low & 0b11);
    state->acceleration.value.y =
        Common::ExpandValue<u16>((accel_y_high << 1) | Common::ExtractBit<2>(accel_low), 1);
    state->acceleration.value.z =
        Common::ExpandValue<u16>((accel_z_high << 1) | Common::ExtractBit<3>(accel_low), 1);
    pos += 4;
  }

  if (has_camera)
  {
    for (size_t i = 0; i < 2; ++i)
    {
      const u8 camera_misc = d[pos];
      const u8 camera_x_high = d[pos + 1];
      const u8 camera_y_high = d[pos + 2];
      const u16 camera_x = (camera_x_high << 2) | (camera_misc & 0b11);
      const u16 camera_y = (camera_y_high << 2) | ((camera_misc >> 2) & 0b11);
      const u8 camera_size = camera_misc >> 4;
      if (camera_y < CameraLogic::CAMERA_RES_Y)
      {
        state->camera_points[i] = CameraPoint({camera_x, camera_y}, camera_size);
      }
      else
      {
        // indicates an invalid camera point
        state->camera_points[i] = CameraPoint();
      }
      pos += 3;
    }
  }

  if (has_motion_plus)
  {
    const u16 pitch_value = d[pos] | ((d[pos + 1] & 0x3f) << 8);
    const u16 roll_value = d[pos + 2] | ((d[pos + 3] & 0x3f) << 8);
    const u16 yaw_value = d[pos + 4] | ((d[pos + 5] & 0x3f) << 8);
    const bool pitch_slow = (d[pos + 1] & 0x80) != 0;
    const bool roll_slow = (d[pos + 3] & 0x80) != 0;
    const bool yaw_slow = (d[pos + 5] & 0x80) != 0;
    state->motion_plus = MotionPlus::DataFormat::Data{
        MotionPlus::DataFormat::GyroRawValue{
            MotionPlus::DataFormat::GyroType(pitch_value, roll_value, yaw_value)},
        MotionPlus::DataFormat::SlowType(pitch_slow, roll_slow, yaw_slow)};
    pos += 6;
  }

  switch (extension)
  {
  case ExtensionNumber::NONE:
    return true;
  case ExtensionNumber::NUNCHUK:
    return DeserializeExtensionState<Nunchuk::DataFormat>(state, serialized, pos);
  case ExtensionNumber::CLASSIC:
    return DeserializeExtensionState<Classic::DataFormat>(state, serialized, pos);
  case ExtensionNumber::GUITAR:
    return DeserializeExtensionState<Guitar::DataFormat>(state, serialized, pos);
  case ExtensionNumber::DRUMS:
    return DeserializeExtensionState<Drums::DesiredState>(state, serialized, pos);
  case ExtensionNumber::TURNTABLE:
    return DeserializeExtensionState<Turntable::DataFormat>(state, serialized, pos);
  case ExtensionNumber::UDRAW_TABLET:
    return DeserializeExtensionState<UDrawTablet::DataFormat>(state, serialized, pos);
  case ExtensionNumber::DRAWSOME_TABLET:
    return DeserializeExtensionState<DrawsomeTablet::DataFormat>(state, serialized, pos);
  case ExtensionNumber::TATACON:
    return DeserializeExtensionState<TaTaCon::DataFormat>(state, serialized, pos);
  case ExtensionNumber::SHINKANSEN:
    return DeserializeExtensionState<Shinkansen::DesiredState>(state, serialized, pos);
  default:
    break;
  }

  return false;
}
}  // namespace WiimoteEmu
