// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Mapping/MappingCommon.h"

#include <QRegExp>
#include <QString>

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerInterface/Device.h"

namespace MappingCommon
{
constexpr int INPUT_DETECT_TIME = 3000;
constexpr int OUTPUT_DETECT_TIME = 2000;

QString GetExpressionForControl(const QString& control_name,
                                const ciface::Core::DeviceQualifier& control_device,
                                const ciface::Core::DeviceQualifier& default_device, Quote quote)
{
  QString expr;

  // non-default device
  if (control_device != default_device)
  {
    expr += QString::fromStdString(control_device.ToString());
    expr += QStringLiteral(":");
  }

  // append the control name
  expr += control_name;

  if (quote == Quote::On)
  {
    QRegExp reg(QStringLiteral("[a-zA-Z]+"));
    if (!reg.exactMatch(expr))
      expr = QStringLiteral("`%1`").arg(expr);
  }

  return expr;
}

QString DetectExpression(ControlReference* reference, ciface::Core::Device* device,
                         const ciface::Core::DeviceQualifier& default_device, Quote quote)
{
  const int ms = reference->IsInput() ? INPUT_DETECT_TIME : OUTPUT_DETECT_TIME;

  ciface::Core::Device::Control* const ctrl = reference->Detect(ms, device);

  if (ctrl)
  {
    return MappingCommon::GetExpressionForControl(QString::fromStdString(ctrl->GetName()),
                                                  default_device, default_device, quote);
  }
  return QStringLiteral("");
}
}  // namespace MappingCommon
