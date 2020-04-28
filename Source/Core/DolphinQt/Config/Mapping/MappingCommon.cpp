// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Mapping/MappingCommon.h"

#include <tuple>

#include <QApplication>
#include <QPushButton>
#include <QRegExp>
#include <QString>
#include <QTimer>

#include "DolphinQt/QtUtils/BlockUserInputFilter.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"
#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerInterface/Device.h"

#include "Common/Thread.h"

namespace MappingCommon
{
constexpr int INPUT_DETECT_TIME = 3000;
constexpr int OUTPUT_TEST_TIME = 2000;

QString GetExpressionForControl(const QString& control_name,
                                const ciface::Core::DeviceQualifier& control_device,
                                const ciface::Core::DeviceQualifier& default_device, Quote quote)
{
  QString expr;

  // non-default device
  if (control_device != default_device)
  {
    expr += QString::fromStdString(control_device.ToString());
    expr += QLatin1Char{':'};
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

QString DetectExpression(QWidget* widget, ciface::Core::DeviceContainer& device_container,
                         const std::vector<std::string>& device_strings,
                         const ciface::Core::DeviceQualifier& default_device, Quote quote)
{
  const auto filter = new BlockUserInputFilter(widget);
  QueueOnObject(widget, [widget, filter] {
    widget->installEventFilter(filter);
    widget->grabKeyboard();
    widget->grabMouse();
  });

  // The button text won't be updated if we don't process events here
  QApplication::processEvents();

  // Avoid that the button press itself is registered as an event
  Common::SleepCurrentThread(50);

  const auto [device, input] = device_container.DetectInput(INPUT_DETECT_TIME, device_strings);

  const auto timer = new QTimer(widget);

  widget->connect(timer, &QTimer::timeout, [widget, filter] {
    widget->releaseMouse();
    widget->releaseKeyboard();
    widget->removeEventFilter(filter);
  });

  // Prevent mappings of "space", "return", or mouse clicks from re-activating detection.
  timer->start(500);

  if (!input)
    return {};

  ciface::Core::DeviceQualifier device_qualifier;
  device_qualifier.FromDevice(device.get());

  return MappingCommon::GetExpressionForControl(QString::fromStdString(input->GetName()),
                                                device_qualifier, default_device, quote);
}

void TestOutput(QPushButton* button, OutputReference* reference)
{
  const auto old_text = button->text();
  button->setText(QStringLiteral("..."));

  // The button text won't be updated if we don't process events here
  QApplication::processEvents();

  reference->State(1.0);
  Common::SleepCurrentThread(OUTPUT_TEST_TIME);
  reference->State(0.0);

  button->setText(old_text);
}

}  // namespace MappingCommon
