// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include "Common/Matrix.h"
#include "InputCommon/ControllerEmu/StickGate.h"
#include "InputCommon/ControllerInterface/CoreDevice.h"

namespace ControllerEmu
{
class EmulatedController;
}  // namespace ControllerEmu

namespace ciface::MappingCommon
{
enum class Quote
{
  On,
  Off
};

std::string GetExpressionForControl(const std::string& control_name,
                                    const Core::DeviceQualifier& control_device,
                                    const Core::DeviceQualifier& default_device,
                                    Quote quote = Quote::On);

std::string BuildExpression(const Core::InputDetector::Results&,
                            const Core::DeviceQualifier& default_device, Quote quote);

void RemoveSpuriousTriggerCombinations(Core::InputDetector::Results*);
void RemoveDetectionsAfterTimePoint(Core::InputDetector::Results*, Clock::time_point after);
bool ContainsCompleteDetection(const Core::InputDetector::Results&);

// class for detecting four directional input mappings in sequence.
class ReshapableInputMapper
{
public:
  // Four cardinal directions.
  static constexpr std::size_t REQUIRED_INPUT_COUNT = 4;

  // Caller should hold the "StateLock".
  ReshapableInputMapper(const Core::DeviceContainer& container,
                        std::span<const std::string> device_strings);

  // Reads inputs and updates internal state.
  // Returns true if an input was detected in this call.
  //  (useful for UI animation)
  // Caller should hold the "StateLock".
  bool Update();

  // A counter-clockwise angle in radians for the currently desired input direction.
  // Used for a graphical indicator in the UI.
  // 0 == East
  float GetCurrentAngle() const;

  // True if all four directions have been detected or the timer expired.
  bool IsComplete() const;

  // Returns true if "analog" inputs were detected and calibration should be performed.
  // Must use *before* ApplyResults.
  bool IsCalibrationNeeded() const;

  // Use when IsComplete returns true.
  // Updates the mappings on the provided ReshapableInput.
  // Caller should hold the "StateLock".
  bool ApplyResults(ControllerEmu::EmulatedController*, ControllerEmu::ReshapableInput* stick);

private:
  Core::InputDetector m_input_detector;
};

class CalibrationBuilder
{
public:
  // Provide nullopt if you want to calibrate the center on first Update.
  explicit CalibrationBuilder(std::optional<Common::DVec2> center = Common::DVec2{});

  // Updates the calibration data using the provided point and the previous point.
  void Update(Common::DVec2 point);

  // Returns true when the calibration data seems to be reasonably filled in.
  // Used to update the UI to encourage the user to click the "Finish" button.
  bool IsCalibrationDataSensible() const;

  // Returns true when the calibration data seems sensible,
  //  and the input then approaches the center position.
  bool IsComplete() const;

  // Grabs the calibration value at the provided angle.
  // Used to render the calibration in the UI while it's in progress.
  ControlState GetCalibrationRadiusAtAngle(double angle) const;

  // Sets the calibration data of the provided ReshapableInput.
  // Caller should hold the "StateLock".
  void ApplyResults(ControllerEmu::ReshapableInput* stick);

  Common::DVec2 GetCenter() const;

private:
  ControllerEmu::ReshapableInput::CalibrationData m_calibration_data;

  std::optional<Common::DVec2> m_center = std::nullopt;

  Common::DVec2 m_prev_point{};
};

}  // namespace ciface::MappingCommon
