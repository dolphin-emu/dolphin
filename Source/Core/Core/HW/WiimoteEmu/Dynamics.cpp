// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/WiimoteEmu/Dynamics.h"

#include <cmath>

#include "Common/MathUtil.h"
#include "Core/Config/WiimoteInputSettings.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "InputCommon/ControllerEmu/ControlGroup/Buttons.h"
#include "InputCommon/ControllerEmu/ControlGroup/Force.h"
#include "InputCommon/ControllerEmu/ControlGroup/Tilt.h"

namespace WiimoteEmu
{
constexpr int SHAKE_FREQ = 6;
// Frame count of one up/down shake
// < 9 no shake detection in "Wario Land: Shake It"
constexpr int SHAKE_STEP_MAX = ::Wiimote::UPDATE_FREQ / SHAKE_FREQ;

void EmulateShake(NormalizedAccelData* const accel, ControllerEmu::Buttons* const buttons_group,
                  const double intensity, u8* const shake_step)
{
  // shake is a bitfield of X,Y,Z shake button states
  static const unsigned int btns[] = {0x01, 0x02, 0x04};
  unsigned int shake = 0;
  buttons_group->GetState(&shake, btns);

  for (int i = 0; i != 3; ++i)
  {
    if (shake & (1 << i))
    {
      (&(accel->x))[i] += std::sin(MathUtil::TAU * shake_step[i] / SHAKE_STEP_MAX) * intensity;
      shake_step[i] = (shake_step[i] + 1) % SHAKE_STEP_MAX;
    }
    else
    {
      shake_step[i] = 0;
    }
  }
}

void EmulateDynamicShake(NormalizedAccelData* const accel, DynamicData& dynamic_data,
                         ControllerEmu::Buttons* const buttons_group,
                         const DynamicConfiguration& config, u8* const shake_step)
{
  // shake is a bitfield of X,Y,Z shake button states
  static const unsigned int btns[] = {0x01, 0x02, 0x04};
  unsigned int shake = 0;
  buttons_group->GetState(&shake, btns);

  for (int i = 0; i != 3; ++i)
  {
    if ((shake & (1 << i)) && dynamic_data.executing_frames_left[i] == 0)
    {
      dynamic_data.timing[i]++;
    }
    else if (dynamic_data.executing_frames_left[i] > 0)
    {
      (&(accel->x))[i] +=
          std::sin(MathUtil::TAU * shake_step[i] / SHAKE_STEP_MAX) * dynamic_data.intensity[i];
      shake_step[i] = (shake_step[i] + 1) % SHAKE_STEP_MAX;
      dynamic_data.executing_frames_left[i]--;
    }
    else if (shake == 0 && dynamic_data.timing[i] > 0)
    {
      if (dynamic_data.timing[i] > config.frames_needed_for_high_intensity)
      {
        dynamic_data.intensity[i] = config.high_intensity;
      }
      else if (dynamic_data.timing[i] < config.frames_needed_for_low_intensity)
      {
        dynamic_data.intensity[i] = config.low_intensity;
      }
      else
      {
        dynamic_data.intensity[i] = config.med_intensity;
      }
      dynamic_data.timing[i] = 0;
      dynamic_data.executing_frames_left[i] = config.frames_to_execute;
    }
    else
    {
      shake_step[i] = 0;
    }
  }
}

void EmulateTilt(NormalizedAccelData* const accel, ControllerEmu::Tilt* const tilt_group,
                 const bool sideways, const bool upright)
{
  // 180 degrees
  const ControllerEmu::Tilt::StateData state = tilt_group->GetState();
  const ControlState roll = state.x * MathUtil::PI;
  const ControlState pitch = state.y * MathUtil::PI;

  // Some notes that no one will understand but me :p
  // left, forward, up
  // lr/ left == negative for all orientations
  // ud/ up == negative for upright longways
  // fb/ forward == positive for (sideways flat)

  // Determine which axis is which direction
  const u32 ud = upright ? (sideways ? 0 : 1) : 2;
  const u32 lr = sideways;
  const u32 fb = upright ? 2 : (sideways ? 0 : 1);

  // Sign fix
  std::array<int, 3> sgn{{-1, 1, 1}};
  if (sideways && !upright)
    sgn[fb] *= -1;
  if (!sideways && upright)
    sgn[ud] *= -1;

  (&accel->x)[ud] = (sin((MathUtil::PI / 2) - std::max(fabs(roll), fabs(pitch)))) * sgn[ud];
  (&accel->x)[lr] = -sin(roll) * sgn[lr];
  (&accel->x)[fb] = sin(pitch) * sgn[fb];
}

void EmulateSwing(NormalizedAccelData* const accel, ControllerEmu::Force* const swing_group,
                  const double intensity, const bool sideways, const bool upright)
{
  const ControllerEmu::Force::StateData swing = swing_group->GetState();

  // Determine which axis is which direction
  const std::array<int, 3> axis_map{{
      upright ? (sideways ? 0 : 1) : 2,  // up/down
      sideways,                          // left/right
      upright ? 2 : (sideways ? 0 : 1),  // forward/backward
  }};

  // Some orientations have up as positive, some as negative
  // same with forward
  std::array<s8, 3> g_dir{{-1, -1, -1}};
  if (sideways && !upright)
    g_dir[axis_map[2]] *= -1;
  if (!sideways && upright)
    g_dir[axis_map[0]] *= -1;

  for (std::size_t i = 0; i < swing.size(); ++i)
    (&accel->x)[axis_map[i]] += swing[i] * g_dir[i] * intensity;
}

void EmulateDynamicSwing(NormalizedAccelData* const accel, DynamicData& dynamic_data,
                         ControllerEmu::Force* const swing_group,
                         const DynamicConfiguration& config, const bool sideways,
                         const bool upright)
{
  const ControllerEmu::Force::StateData swing = swing_group->GetState();

  // Determine which axis is which direction
  const std::array<int, 3> axis_map{{
      upright ? (sideways ? 0 : 1) : 2,  // up/down
      sideways,                          // left/right
      upright ? 2 : (sideways ? 0 : 1),  // forward/backward
  }};

  // Some orientations have up as positive, some as negative
  // same with forward
  std::array<s8, 3> g_dir{{-1, -1, -1}};
  if (sideways && !upright)
    g_dir[axis_map[2]] *= -1;
  if (!sideways && upright)
    g_dir[axis_map[0]] *= -1;

  for (std::size_t i = 0; i < swing.size(); ++i)
  {
    if (swing[i] > 0 && dynamic_data.executing_frames_left[i] == 0)
    {
      dynamic_data.timing[i]++;
    }
    else if (dynamic_data.executing_frames_left[i] > 0)
    {
      (&accel->x)[axis_map[i]] += g_dir[i] * dynamic_data.intensity[i];
      dynamic_data.executing_frames_left[i]--;
    }
    else if (swing[i] == 0 && dynamic_data.timing[i] > 0)
    {
      if (dynamic_data.timing[i] > config.frames_needed_for_high_intensity)
      {
        dynamic_data.intensity[i] = config.high_intensity;
      }
      else if (dynamic_data.timing[i] < config.frames_needed_for_low_intensity)
      {
        dynamic_data.intensity[i] = config.low_intensity;
      }
      else
      {
        dynamic_data.intensity[i] = config.med_intensity;
      }
      dynamic_data.timing[i] = 0;
      dynamic_data.executing_frames_left[i] = config.frames_to_execute;
    }
  }
}

WiimoteCommon::DataReportBuilder::AccelData DenormalizeAccelData(const NormalizedAccelData& accel,
                                                                 u16 zero_g, u16 one_g)
{
  const u8 accel_range = one_g - zero_g;

  const s32 unclamped_x = (s32)(accel.x * accel_range + zero_g);
  const s32 unclamped_y = (s32)(accel.y * accel_range + zero_g);
  const s32 unclamped_z = (s32)(accel.z * accel_range + zero_g);

  WiimoteCommon::DataReportBuilder::AccelData result;

  result.x = MathUtil::Clamp<u16>(unclamped_x, 0, 0x3ff);
  result.y = MathUtil::Clamp<u16>(unclamped_y, 0, 0x3ff);
  result.z = MathUtil::Clamp<u16>(unclamped_z, 0, 0x3ff);

  return result;
}

void Wiimote::GetAccelData(NormalizedAccelData* accel)
{
  const bool is_sideways = IsSideways();
  const bool is_upright = IsUpright();

  EmulateTilt(accel, m_tilt, is_sideways, is_upright);

  DynamicConfiguration swing_config;
  swing_config.low_intensity = Config::Get(Config::WIIMOTE_INPUT_SWING_INTENSITY_SLOW);
  swing_config.med_intensity = Config::Get(Config::WIIMOTE_INPUT_SWING_INTENSITY_MEDIUM);
  swing_config.high_intensity = Config::Get(Config::WIIMOTE_INPUT_SWING_INTENSITY_FAST);
  swing_config.frames_needed_for_high_intensity =
      Config::Get(Config::WIIMOTE_INPUT_SWING_DYNAMIC_FRAMES_HELD_FAST);
  swing_config.frames_needed_for_low_intensity =
      Config::Get(Config::WIIMOTE_INPUT_SWING_DYNAMIC_FRAMES_HELD_SLOW);
  swing_config.frames_to_execute = Config::Get(Config::WIIMOTE_INPUT_SWING_DYNAMIC_FRAMES_LENGTH);

  EmulateSwing(accel, m_swing, Config::Get(Config::WIIMOTE_INPUT_SWING_INTENSITY_MEDIUM),
               is_sideways, is_upright);
  EmulateSwing(accel, m_swing_slow, Config::Get(Config::WIIMOTE_INPUT_SWING_INTENSITY_SLOW),
               is_sideways, is_upright);
  EmulateSwing(accel, m_swing_fast, Config::Get(Config::WIIMOTE_INPUT_SWING_INTENSITY_FAST),
               is_sideways, is_upright);
  EmulateDynamicSwing(accel, m_swing_dynamic_data, m_swing_dynamic, swing_config, is_sideways,
                      is_upright);

  DynamicConfiguration shake_config;
  shake_config.low_intensity = Config::Get(Config::WIIMOTE_INPUT_SHAKE_INTENSITY_SOFT);
  shake_config.med_intensity = Config::Get(Config::WIIMOTE_INPUT_SHAKE_INTENSITY_MEDIUM);
  shake_config.high_intensity = Config::Get(Config::WIIMOTE_INPUT_SHAKE_INTENSITY_HARD);
  shake_config.frames_needed_for_high_intensity =
      Config::Get(Config::WIIMOTE_INPUT_SHAKE_DYNAMIC_FRAMES_HELD_HARD);
  shake_config.frames_needed_for_low_intensity =
      Config::Get(Config::WIIMOTE_INPUT_SHAKE_DYNAMIC_FRAMES_HELD_SOFT);
  shake_config.frames_to_execute = Config::Get(Config::WIIMOTE_INPUT_SHAKE_DYNAMIC_FRAMES_LENGTH);

  EmulateShake(accel, m_shake, Config::Get(Config::WIIMOTE_INPUT_SHAKE_INTENSITY_MEDIUM),
               m_shake_step.data());
  EmulateShake(accel, m_shake_soft, Config::Get(Config::WIIMOTE_INPUT_SHAKE_INTENSITY_SOFT),
               m_shake_soft_step.data());
  EmulateShake(accel, m_shake_hard, Config::Get(Config::WIIMOTE_INPUT_SHAKE_INTENSITY_HARD),
               m_shake_hard_step.data());
  EmulateDynamicShake(accel, m_shake_dynamic_data, m_shake_dynamic, shake_config,
                      m_shake_dynamic_step.data());
}

}  // namespace WiimoteEmu
