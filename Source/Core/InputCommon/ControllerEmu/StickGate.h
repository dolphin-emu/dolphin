// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <optional>
#include <vector>

#include "Common/Matrix.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

namespace ControllerEmu
{
// An abstract class representing the plastic shell that limits an analog stick's movement.
class StickGate
{
public:
  // Angle is in radians and should be non-negative
  virtual ControlState GetRadiusAtAngle(double ang) const = 0;

  // This is provided purely as an optimization for ReshapableInput to produce a minimal amount of
  // calibration points that are saved in our config.
  virtual std::optional<u32> GetIdealCalibrationSampleCount() const;

  virtual ~StickGate() = default;
};

// An octagon-shaped stick gate is found on most Nintendo GC/Wii analog sticks.
class OctagonStickGate : public StickGate
{
public:
  // Radius of circumscribed circle
  explicit OctagonStickGate(ControlState radius);
  ControlState GetRadiusAtAngle(double ang) const override final;
  std::optional<u32> GetIdealCalibrationSampleCount() const override final;

private:
  const ControlState m_radius;
};

// A round-shaped stick gate. Possibly found on 3rd-party accessories.
class RoundStickGate : public StickGate
{
public:
  explicit RoundStickGate(ControlState radius);
  ControlState GetRadiusAtAngle(double ang) const override final;

private:
  const ControlState m_radius;
};

// A square-shaped stick gate. e.g. keyboard input.
class SquareStickGate : public StickGate
{
public:
  explicit SquareStickGate(ControlState half_width);
  ControlState GetRadiusAtAngle(double ang) const override final;
  std::optional<u32> GetIdealCalibrationSampleCount() const override final;

private:
  const ControlState m_half_width;
};

class ReshapableInput : public ControlGroup
{
public:
  // This is the number of samples we generate but any number could be loaded from config.
  static constexpr int CALIBRATION_SAMPLE_COUNT = 32;

  // Contains input radius maximums at evenly-spaced angles.
  using CalibrationData = std::vector<ControlState>;

  ReshapableInput(std::string name, std::string ui_name, GroupType type);

  using ReshapeData = Common::DVec2;

  // Angle is in radians and should be non-negative
  ControlState GetDeadzoneRadiusAtAngle(double angle) const;
  ControlState GetInputRadiusAtAngle(double angle) const;

  ControlState GetDeadzonePercentage() const;

  virtual ControlState GetGateRadiusAtAngle(double angle) const = 0;
  virtual ReshapeData GetReshapableState(bool adjusted) = 0;
  virtual ControlState GetDefaultInputRadiusAtAngle(double ang) const;

  void SetCalibrationToDefault();
  void SetCalibrationFromGate(const StickGate& gate);

  static void UpdateCalibrationData(CalibrationData& data, Common::DVec2 point);
  static ControlState GetCalibrationDataRadiusAtAngle(const CalibrationData& data, double angle);

  const CalibrationData& GetCalibrationData() const;
  void SetCalibrationData(CalibrationData data);

  const ReshapeData& GetCenter() const;
  void SetCenter(ReshapeData center);

protected:
  ReshapeData Reshape(ControlState x, ControlState y, ControlState modifier = 0.0);

private:
  void LoadConfig(IniFile::Section*, const std::string&, const std::string&) override;
  void SaveConfig(IniFile::Section*, const std::string&, const std::string&) override;

  CalibrationData m_calibration;
  SettingValue<double> m_deadzone_setting;
  ReshapeData m_center;
};

}  // namespace ControllerEmu
