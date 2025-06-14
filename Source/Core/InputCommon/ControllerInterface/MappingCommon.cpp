// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/MappingCommon.h"

#include <algorithm>
#include <chrono>
#include <ranges>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include "Common/MathUtil.h"
#include "Common/StringUtil.h"

#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/StickGate.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/CoreDevice.h"
#include "InputCommon/InputConfig.h"

namespace ciface::MappingCommon
{
// Pressing inputs at the same time will result in the & operator vs a hotkey expression.
constexpr auto HOTKEY_VS_CONJUNCION_THRESHOLD = std::chrono::milliseconds(50);

// Some devices (e.g. DS4) provide an analog and digital input for the trigger.
// We prefer just the analog input for simultaneous digital+analog input detections.
constexpr auto SPURIOUS_TRIGGER_COMBO_THRESHOLD = std::chrono::milliseconds(150);

std::string GetExpressionForControl(const std::string& control_name,
                                    const ciface::Core::DeviceQualifier& control_device,
                                    const ciface::Core::DeviceQualifier& default_device,
                                    Quote quote)
{
  std::string expr;

  // non-default device
  if (control_device != default_device)
  {
    expr += control_device.ToString();
    expr += ':';
  }

  // append the control name
  expr += control_name;

  if (quote == Quote::On)
  {
    // If our expression contains any non-alpha characters
    // we should quote it
    if (!std::ranges::all_of(expr, Common::IsAlpha))
      expr = fmt::format("`{}`", expr);
  }

  return expr;
}

std::string BuildExpression(const Core::InputDetector::Results& detections,
                            const ciface::Core::DeviceQualifier& default_device, Quote quote)
{
  std::vector<const Core::InputDetector::Detection*> pressed_inputs;

  std::vector<std::string> alternations;

  const auto get_control_expression = [&](auto& detection) {
    // Return the parent-most name if there is one for better hotkey strings.
    // Detection of L/R_Ctrl will be changed to just Ctrl.
    // Users can manually map L_Ctrl if they so desire.
    const auto input = (quote == Quote::On) ?
                           detection.device->GetParentMostInput(detection.input) :
                           detection.input;

    ciface::Core::DeviceQualifier device_qualifier;
    device_qualifier.FromDevice(detection.device.get());

    return MappingCommon::GetExpressionForControl(input->GetName(), device_qualifier,
                                                  default_device, quote);
  };

  bool new_alternation = false;

  const auto handle_press = [&](auto& detection) {
    pressed_inputs.emplace_back(&detection);
    new_alternation = true;
  };

  const auto handle_release = [&] {
    if (!new_alternation)
      return;

    new_alternation = false;

    std::vector<std::string> alternation;
    for (auto* input : pressed_inputs)
      alternation.push_back(get_control_expression(*input));

    const bool is_hotkey = pressed_inputs.size() >= 2 &&
                           (pressed_inputs[1]->press_time - pressed_inputs[0]->press_time) >
                               HOTKEY_VS_CONJUNCION_THRESHOLD;

    if (is_hotkey)
    {
      alternations.push_back(fmt::format("@({})", fmt::join(alternation, "+")));
    }
    else
    {
      std::ranges::sort(alternation);
      alternations.push_back(fmt::to_string(fmt::join(alternation, "&")));
    }
  };

  for (auto& detection : detections)
  {
    // Remove since-released inputs.
    for (auto it = pressed_inputs.begin(); it != pressed_inputs.end();)
    {
      if ((*it)->release_time && (*it)->release_time <= detection.press_time)
      {
        handle_release();
        it = pressed_inputs.erase(it);
      }
      else
      {
        ++it;
      }
    }

    handle_press(detection);
  }

  handle_release();

  // Remove duplicates
  std::ranges::sort(alternations);
  const auto unique_result = std::ranges::unique(alternations);
  alternations.erase(unique_result.begin(), unique_result.end());

  return fmt::to_string(fmt::join(alternations, "|"));
}

void RemoveSpuriousTriggerCombinations(Core::InputDetector::Results* detections)
{
  const auto is_spurious = [&](const auto& detection) {
    return std::ranges::any_of(*detections, [&](const auto& d) {
      // This is a spurious digital detection if a "smooth" (analog) detection is temporally near.
      return &d != &detection && d.IsAnalogPress() && !detection.IsAnalogPress() &&
             abs(d.press_time - detection.press_time) < SPURIOUS_TRIGGER_COMBO_THRESHOLD;
    });
  };

  std::erase_if(*detections, is_spurious);
}

void RemoveDetectionsAfterTimePoint(Core::InputDetector::Results* results, Clock::time_point after)
{
  const auto is_after_time = [&](const Core::InputDetector::Detection& detection) {
    return detection.release_time.value_or(after) >= after;
  };

  std::erase_if(*results, is_after_time);
}

bool ContainsCompleteDetection(const Core::InputDetector::Results& results)
{
  return std::ranges::any_of(results, [](const Core::InputDetector::Detection& detection) {
    return detection.release_time.has_value();
  });
}

ReshapableInputMapper::ReshapableInputMapper(const Core::DeviceContainer& container,
                                             std::span<const std::string> device_strings)
{
  m_input_detector.Start(container, device_strings);
}

bool ReshapableInputMapper::Update()
{
  const auto prev_size = m_input_detector.GetResults().size();

  constexpr auto wait_time = std::chrono::seconds{4};
  m_input_detector.Update(wait_time, wait_time, wait_time * REQUIRED_INPUT_COUNT);

  return m_input_detector.GetResults().size() != prev_size;
}

float ReshapableInputMapper::GetCurrentAngle() const
{
  constexpr auto quarter_circle = float(MathUtil::TAU) * 0.25f;
  return quarter_circle - (float(m_input_detector.GetResults().size()) * quarter_circle);
}

bool ReshapableInputMapper::IsComplete() const
{
  return m_input_detector.GetResults().size() >= REQUIRED_INPUT_COUNT ||
         m_input_detector.IsComplete();
}

bool ReshapableInputMapper::IsCalibrationNeeded() const
{
  return std::ranges::any_of(m_input_detector.GetResults() | std::views::take(REQUIRED_INPUT_COUNT),
                             &ciface::Core::InputDetector::Detection::IsAnalogPress);
}

bool ReshapableInputMapper::ApplyResults(ControllerEmu::EmulatedController* controller,
                                         ControllerEmu::ReshapableInput* stick)
{
  auto const detections = m_input_detector.TakeResults();

  if (detections.size() < REQUIRED_INPUT_COUNT)
    return false;

  // Transpose URDL to UDLR.
  const std::array results{detections[0], detections[2], detections[3], detections[1]};

  const auto default_device = controller->GetDefaultDevice();

  for (std::size_t i = 0; i != results.size(); ++i)
  {
    ciface::Core::DeviceQualifier device_qualifier;
    device_qualifier.FromDevice(results[i].device.get());

    stick->controls[i]->control_ref->SetExpression(ciface::MappingCommon::GetExpressionForControl(
        results[i].input->GetName(), device_qualifier, default_device,
        ciface::MappingCommon::Quote::On));

    controller->UpdateSingleControlReference(g_controller_interface,
                                             stick->controls[i]->control_ref.get());
  }

  controller->GetConfig()->GenerateControllerTextures();

  return true;
}

CalibrationBuilder::CalibrationBuilder(std::optional<Common::DVec2> center)
    : m_calibration_data(ControllerEmu::ReshapableInput::CALIBRATION_SAMPLE_COUNT, 0.0),
      m_center{center}
{
}

void CalibrationBuilder::Update(Common::DVec2 point)
{
  if (!m_center.has_value())
    m_center = point;

  const auto new_point = point - *m_center;
  ControllerEmu::ReshapableInput::UpdateCalibrationData(m_calibration_data, m_prev_point,
                                                        new_point);
  m_prev_point = new_point;
}

bool CalibrationBuilder::IsCalibrationDataSensible() const
{
  // Even the GC controller's small range would pass this test.
  constexpr double REASONABLE_AVERAGE_RADIUS = 0.6;

  // Test that the average input radius is not below a threshold.
  // This will make sure the user has actually moved their stick from neutral.

  MathUtil::RunningVariance<ControlState> stats;

  for (const auto x : m_calibration_data)
    stats.Push(x);

  if (stats.Mean() < REASONABLE_AVERAGE_RADIUS)
    return false;

  // Test that the standard deviation is below a threshold.
  // This will make sure the user has not just filled in one side of their input.

  // Approx. deviation of a square input gate, anything much more than that would be unusual.
  constexpr double REASONABLE_DEVIATION = 0.14;

  return stats.StandardDeviation() < REASONABLE_DEVIATION;
}

ControlState CalibrationBuilder::GetCalibrationRadiusAtAngle(double angle) const
{
  return ControllerEmu::ReshapableInput::GetCalibrationDataRadiusAtAngle(m_calibration_data, angle);
}

void CalibrationBuilder::ApplyResults(ControllerEmu::ReshapableInput* stick)
{
  stick->SetCenter(GetCenter());
  stick->SetCalibrationData(std::move(m_calibration_data));
}

Common::DVec2 CalibrationBuilder::GetCenter() const
{
  return m_center.value_or(Common::DVec2{});
}

}  // namespace ciface::MappingCommon
