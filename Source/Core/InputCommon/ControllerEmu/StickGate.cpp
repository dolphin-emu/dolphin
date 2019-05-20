// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/StickGate.h"

#include <algorithm>
#include <cmath>

#include "Common/Common.h"
#include "Common/MathUtil.h"
#include "Common/Matrix.h"
#include "Common/StringUtil.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

namespace
{
constexpr auto CALIBRATION_CONFIG_NAME = "Calibration";
constexpr auto CALIBRATION_DEFAULT_VALUE = 1.0;
constexpr auto CALIBRATION_CONFIG_SCALE = 100;

constexpr auto CENTER_CONFIG_NAME = "Center";
constexpr auto CENTER_CONFIG_SCALE = 100;

// Calculate distance to intersection of a ray with a line defined by two points.
double GetRayLineIntersection(Common::DVec2 ray, Common::DVec2 point1, Common::DVec2 point2)
{
  const auto diff = point2 - point1;

  const auto dot = diff.Dot({-ray.y, ray.x});
  if (std::abs(dot) < 0.00001)
  {
    // Handle situation where both points are on top of eachother.
    // This could occur if the user configures a single calibration value
    // or when updating calibration.
    return point1.Length();
  }

  return diff.Cross(-point1) / dot;
}

Common::DVec2 GetPointFromAngleAndLength(double angle, double length)
{
  return Common::DVec2{std::cos(angle), std::sin(angle)} * length;
}
}  // namespace

namespace ControllerEmu
{
constexpr int ReshapableInput::CALIBRATION_SAMPLE_COUNT;

std::optional<u32> StickGate::GetIdealCalibrationSampleCount() const
{
  return {};
}

OctagonStickGate::OctagonStickGate(ControlState radius) : m_radius(radius)
{
}

ControlState OctagonStickGate::GetRadiusAtAngle(double angle) const
{
  constexpr int sides = 8;
  constexpr double sum_int_angles = (sides - 2) * MathUtil::PI;
  constexpr double half_int_angle = sum_int_angles / sides / 2;

  angle = std::fmod(angle, MathUtil::TAU / sides);
  // Solve ASA triangle using The Law of Sines:
  return m_radius / std::sin(MathUtil::PI - angle - half_int_angle) * std::sin(half_int_angle);
}

std::optional<u32> OctagonStickGate::GetIdealCalibrationSampleCount() const
{
  return 8;
}

RoundStickGate::RoundStickGate(ControlState radius) : m_radius(radius)
{
}

ControlState RoundStickGate::GetRadiusAtAngle(double) const
{
  return m_radius;
}

SquareStickGate::SquareStickGate(ControlState half_width) : m_half_width(half_width)
{
}

ControlState SquareStickGate::GetRadiusAtAngle(double angle) const
{
  constexpr double section_angle = MathUtil::TAU / 4;
  return m_half_width /
         std::cos(std::fmod(angle + section_angle / 2, section_angle) - section_angle / 2);
}

std::optional<u32> SquareStickGate::GetIdealCalibrationSampleCount() const
{
  // Because angle:0 points to the right we must use 8 samples for our square.
  return 8;
}

ReshapableInput::ReshapableInput(std::string name, std::string ui_name, GroupType type)
    : ControlGroup(std::move(name), std::move(ui_name), type)
{
  AddDeadzoneSetting(&m_deadzone_setting, 50);
}

ControlState ReshapableInput::GetDeadzoneRadiusAtAngle(double angle) const
{
  // FYI: deadzone is scaled by input radius which allows the shape to match.
  return GetInputRadiusAtAngle(angle) * GetDeadzonePercentage();
}

ControlState ReshapableInput::GetInputRadiusAtAngle(double angle) const
{
  // Handle the "default" state.
  if (m_calibration.empty())
  {
    return GetDefaultInputRadiusAtAngle(angle);
  }

  return GetCalibrationDataRadiusAtAngle(m_calibration, angle);
}

ControlState ReshapableInput::GetDeadzonePercentage() const
{
  return m_deadzone_setting.GetValue() / 100;
}

ControlState ReshapableInput::GetCalibrationDataRadiusAtAngle(const CalibrationData& data,
                                                              double angle)
{
  const auto sample_pos = angle / MathUtil::TAU * data.size();
  // Interpolate the radius between 2 calibration samples.
  const u32 sample1_index = u32(sample_pos) % data.size();
  const u32 sample2_index = (sample1_index + 1) % data.size();
  const double sample1_angle = sample1_index * MathUtil::TAU / data.size();
  const double sample2_angle = sample2_index * MathUtil::TAU / data.size();

  return GetRayLineIntersection(GetPointFromAngleAndLength(angle, 1.0),
                                GetPointFromAngleAndLength(sample1_angle, data[sample1_index]),
                                GetPointFromAngleAndLength(sample2_angle, data[sample2_index]));
}

ControlState ReshapableInput::GetDefaultInputRadiusAtAngle(double angle) const
{
  // This will normally be the same as the gate radius.
  // Unless a sub-class is doing weird things with the gate radius (e.g. Tilt)
  return GetGateRadiusAtAngle(angle);
}

void ReshapableInput::SetCalibrationToDefault()
{
  m_calibration.clear();
}

void ReshapableInput::SetCalibrationFromGate(const StickGate& gate)
{
  m_calibration.resize(gate.GetIdealCalibrationSampleCount().value_or(CALIBRATION_SAMPLE_COUNT));

  u32 i = 0;
  for (auto& val : m_calibration)
    val = gate.GetRadiusAtAngle(MathUtil::TAU * i++ / m_calibration.size());
}

void ReshapableInput::UpdateCalibrationData(CalibrationData& data, Common::DVec2 point)
{
  const auto angle_scale = MathUtil::TAU / data.size();

  const u32 calibration_index =
      std::lround((std::atan2(point.y, point.x) + MathUtil::TAU) / angle_scale) % data.size();
  const double calibration_angle = calibration_index * angle_scale;
  auto& calibration_sample = data[calibration_index];

  // Update closest sample from provided x,y.
  calibration_sample = std::max(calibration_sample, point.Length());

  // Here we update all other samples in our calibration vector to maintain
  // a convex polygon containing our new calibration point.
  // This is required to properly fill in angles that cannot be gotten.
  // (e.g. Keyboard input only has 8 possible angles)

  // Note: Loop assumes an even sample count, which should not be a problem.
  for (auto sample_offset = u32(data.size() / 2 - 1); sample_offset > 1; --sample_offset)
  {
    const auto update_at_offset = [&](u32 offset1, u32 offset2) {
      const u32 sample1_index = (calibration_index + offset1) % data.size();
      const double sample1_angle = sample1_index * angle_scale;
      auto& sample1 = data[sample1_index];

      const u32 sample2_index = (calibration_index + offset2) % data.size();
      const double sample2_angle = sample2_index * angle_scale;
      auto& sample2 = data[sample2_index];

      const double intersection =
          GetRayLineIntersection(GetPointFromAngleAndLength(sample2_angle, 1.0),
                                 GetPointFromAngleAndLength(sample1_angle, sample1),
                                 GetPointFromAngleAndLength(calibration_angle, calibration_sample));

      sample2 = std::max(sample2, intersection);
    };

    update_at_offset(sample_offset, sample_offset - 1);
    update_at_offset(u32(data.size() - sample_offset), u32(data.size() - sample_offset + 1));
  }
}

const ReshapableInput::CalibrationData& ReshapableInput::GetCalibrationData() const
{
  return m_calibration;
}

void ReshapableInput::SetCalibrationData(CalibrationData data)
{
  m_calibration = std::move(data);
}

const ReshapableInput::ReshapeData& ReshapableInput::GetCenter() const
{
  return m_center;
}

void ReshapableInput::SetCenter(ReshapableInput::ReshapeData center)
{
  m_center = center;
}

void ReshapableInput::LoadConfig(IniFile::Section* section, const std::string& default_device,
                                 const std::string& base_name)
{
  ControlGroup::LoadConfig(section, default_device, base_name);

  const std::string group(base_name + name + '/');
  std::string load_str;
  section->Get(group + CALIBRATION_CONFIG_NAME, &load_str, "");
  const auto load_data = SplitString(load_str, ' ');

  m_calibration.assign(load_data.size(), CALIBRATION_DEFAULT_VALUE);

  auto it = load_data.begin();
  for (auto& sample : m_calibration)
  {
    if (TryParse(*(it++), &sample))
      sample /= CALIBRATION_CONFIG_SCALE;
  }

  section->Get(group + CENTER_CONFIG_NAME, &load_str, "");
  const auto center_data = SplitString(load_str, ' ');

  m_center = Common::DVec2();

  if (center_data.size() == 2)
  {
    if (TryParse(center_data[0], &m_center.x))
      m_center.x /= CENTER_CONFIG_SCALE;

    if (TryParse(center_data[1], &m_center.y))
      m_center.y /= CENTER_CONFIG_SCALE;
  }
}

void ReshapableInput::SaveConfig(IniFile::Section* section, const std::string& default_device,
                                 const std::string& base_name)
{
  ControlGroup::SaveConfig(section, default_device, base_name);

  const std::string group(base_name + name + '/');
  std::vector<std::string> save_data(m_calibration.size());
  std::transform(
      m_calibration.begin(), m_calibration.end(), save_data.begin(),
      [](ControlState val) { return StringFromFormat("%.2f", val * CALIBRATION_CONFIG_SCALE); });
  section->Set(group + CALIBRATION_CONFIG_NAME, JoinStrings(save_data, " "), "");

  const auto center_data = StringFromFormat("%.2f %.2f", m_center.x * CENTER_CONFIG_SCALE,
                                            m_center.y * CENTER_CONFIG_SCALE);

  section->Set(group + CENTER_CONFIG_NAME, center_data, "");
}

ReshapableInput::ReshapeData ReshapableInput::Reshape(ControlState x, ControlState y,
                                                      ControlState modifier)
{
  x -= m_center.x;
  y -= m_center.y;

  // TODO: make the AtAngle functions work with negative angles:
  const ControlState angle = std::atan2(y, x) + MathUtil::TAU;

  const ControlState gate_max_dist = GetGateRadiusAtAngle(angle);
  const ControlState input_max_dist = GetInputRadiusAtAngle(angle);

  // If input radius (from calibration) is zero apply no scaling to prevent division by zero.
  const ControlState max_dist = input_max_dist ? input_max_dist : gate_max_dist;

  ControlState dist = Common::DVec2{x, y}.Length() / max_dist;

  // If the modifier is pressed, scale the distance by the modifier's value.
  // This is affected by the modifier's "range" setting which defaults to 50%.
  if (modifier)
  {
    // TODO: Modifier's range setting gets reset to 100% when the clear button is clicked.
    // This causes the modifier to not behave how a user might suspect.
    // Retaining the old scale-by-50% behavior until range is fixed to clear to 50%.
    dist *= 0.5;
    // dist *= modifier;
  }

  // Apply deadzone as a percentage of the user-defined calibration shape/size:
  dist = ApplyDeadzone(dist, GetDeadzonePercentage());

  // Scale to the gate shape/radius:
  dist *= gate_max_dist;

  return {std::clamp(std::cos(angle) * dist, -1.0, 1.0),
          std::clamp(std::sin(angle) * dist, -1.0, 1.0)};
}

}  // namespace ControllerEmu
