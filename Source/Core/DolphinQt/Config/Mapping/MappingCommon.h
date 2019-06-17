// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

class QString;
class OutputReference;
class QPushButton;

namespace ciface::Core
{
class DeviceContainer;
class DeviceQualifier;
}  // namespace ciface::Core

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

}  // namespace MappingCommon
