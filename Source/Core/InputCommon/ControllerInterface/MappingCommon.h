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
                                    const ciface::Core::DeviceQualifier& control_device,
                                    const ciface::Core::DeviceQualifier& default_device,
                                    Quote quote = Quote::On);

std::string BuildExpression(const std::vector<ciface::Core::DeviceContainer::InputDetection>&,
                            const ciface::Core::DeviceQualifier& default_device, Quote quote);

void RemoveSpuriousTriggerCombinations(std::vector<ciface::Core::DeviceContainer::InputDetection>*);

}  // namespace ciface::MappingCommon
