// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/MappingCommon.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

#include <fmt/format.h>

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
    if (!std::all_of(expr.begin(), expr.end(), Common::IsAlpha))
      expr = fmt::format("`{}`", expr);
  }

  return expr;
}

std::string
BuildExpression(const std::vector<ciface::Core::DeviceContainer::InputDetection>& detections,
                const ciface::Core::DeviceQualifier& default_device, Quote quote)
{
  std::vector<const ciface::Core::DeviceContainer::InputDetection*> pressed_inputs;

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
      alternations.push_back(fmt::format("@({})", JoinStrings(alternation, "+")));
    }
    else
    {
      std::sort(alternation.begin(), alternation.end());
      alternations.push_back(JoinStrings(alternation, "&"));
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
  std::sort(alternations.begin(), alternations.end());
  alternations.erase(std::unique(alternations.begin(), alternations.end()), alternations.end());

  return JoinStrings(alternations, "|");
}

void RemoveSpuriousTriggerCombinations(
    std::vector<ciface::Core::DeviceContainer::InputDetection>* detections)
{
  const auto is_spurious = [&](auto& detection) {
    return std::any_of(detections->begin(), detections->end(), [&](auto& d) {
      // This is a suprious digital detection if a "smooth" (analog) detection is temporally near.
      return &d != &detection && d.smoothness > 1 &&
             abs(d.press_time - detection.press_time) < SPURIOUS_TRIGGER_COMBO_THRESHOLD;
    });
  };

  detections->erase(std::remove_if(detections->begin(), detections->end(), is_spurious),
                    detections->end());
}

}  // namespace ciface::MappingCommon
