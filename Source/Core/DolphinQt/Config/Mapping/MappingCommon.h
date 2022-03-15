// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

#include "InputCommon/ControllerInterface/CoreDevice.h"
#include "InputCommon/ControllerInterface/MappingCommon.h"

class QString;
class OutputReference;
class QPushButton;

namespace MappingCommon
{
QString DetectExpression(QPushButton* button, ciface::Core::DeviceContainer& device_container,
                         const std::vector<std::string>& device_strings,
                         const ciface::Core::DeviceQualifier& default_device,
                         ciface::MappingCommon::Quote quote = ciface::MappingCommon::Quote::On);

void TestOutput(QPushButton* button, OutputReference* reference);

}  // namespace MappingCommon
