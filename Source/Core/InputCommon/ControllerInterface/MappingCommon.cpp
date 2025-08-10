// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/MappingCommon.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include "Common/StringUtil.h"
#include "InputCommon/ControllerInterface/CoreDevice.h"

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

  const auto handle_release = [&]() {
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
      return &d != &detection && d.smoothness > 1 && d.smoothness > detection.smoothness &&
             abs(d.press_time - detection.press_time) < SPURIOUS_TRIGGER_COMBO_THRESHOLD;
    });
  };

  std::erase_if(*detections, is_spurious);
}

void RemoveDetectionsAfterTimePoint(Core::InputDetector::Results* results,
                                    Core::DeviceContainer::Clock::time_point after)
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

}  // namespace ciface::MappingCommon
