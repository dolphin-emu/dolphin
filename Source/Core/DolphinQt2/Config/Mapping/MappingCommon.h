// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

class QString;

namespace ciface
{
namespace Core
{
class Device;
class DeviceQualifier;
}
}

namespace ControllerEmu
{
class ControlReference;
};

namespace MappingCommon
{
QString GetExpressionForControl(const QString& control_name,
                                const ciface::Core::DeviceQualifier& control_device,
                                const ciface::Core::DeviceQualifier& default_device);
QString DetectExpression(ControllerEmu::ControlReference* reference, ciface::Core::Device* device,
                         const ciface::Core::DeviceQualifier& m_devq,
                         const ciface::Core::DeviceQualifier& default_device);
}
