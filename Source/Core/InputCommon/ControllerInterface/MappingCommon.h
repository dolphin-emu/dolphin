// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

#include "InputCommon/ControllerInterface/CoreDevice.h"

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
void RemoveDetectionsAfterTimePoint(Core::InputDetector::Results*,
                                    Core::DeviceContainer::Clock::time_point after);
bool ContainsCompleteDetection(const Core::InputDetector::Results&);

}  // namespace ciface::MappingCommon
