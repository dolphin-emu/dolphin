// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

#include "InputCommon/ControllerInterface/CoreDevice.h"

class QString;
class OutputReference;
class QPushButton;

namespace MappingCommon
{
enum class Quote
{
  On,
  Off
};

QString GetExpressionForControl(const QString& control_name,
                                const ciface::Core::DeviceQualifier& control_device,
                                const ciface::Core::DeviceQualifier& default_device,
                                Quote quote = Quote::On);

QString DetectExpression(QPushButton* button, ciface::Core::DeviceContainer& device_container,
                         const std::vector<std::string>& device_strings,
                         const ciface::Core::DeviceQualifier& default_device,
                         Quote quote = Quote::On);

void TestOutput(QPushButton* button, OutputReference* reference);

void RemoveSpuriousTriggerCombinations(std::vector<ciface::Core::DeviceContainer::InputDetection>*);

QString BuildExpression(const std::vector<ciface::Core::DeviceContainer::InputDetection>&,
                        const ciface::Core::DeviceQualifier& default_device, Quote quote);

}  // namespace MappingCommon
