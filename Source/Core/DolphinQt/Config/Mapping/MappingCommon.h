// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

class OutputReference;
class QPushButton;
class QString;
class QWidget;

constexpr int INDICATOR_UPDATE_FREQ = 30;

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

QString DetectExpression(QWidget* widget, ciface::Core::DeviceContainer& device_container,
                         const std::vector<std::string>& device_strings,
                         const ciface::Core::DeviceQualifier& default_device,
                         Quote quote = Quote::On);

void TestOutput(QPushButton* button, OutputReference* reference);

}  // namespace MappingCommon
